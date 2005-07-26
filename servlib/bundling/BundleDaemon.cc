/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Bundle.h"
#include "BundleActions.h"
#include "BundleConsumer.h"
#include "BundleEvent.h"
#include "BundleDaemon.h"
#include "BundleStatusReport.h"
#include "Contact.h"
#include "ContactManager.h"
#include "ExpirationTimer.h"
#include "FragmentManager.h"
#include "reg/Registration.h"
#include "reg/RegistrationTable.h"
#include "routing/BundleRouter.h"

namespace dtn {

BundleDaemon* BundleDaemon::instance_ = NULL;
size_t BundleDaemon::proactive_frag_threshold_;

BundleDaemon::BundleDaemon()
    : BundleEventHandler("/bundle/daemon")
{
    bundles_received_ = 0;
    bundles_delivered_ = 0;
    bundles_transmitted_ = 0;
    bundles_expired_ = 0;

    pending_bundles_ = new BundleList("pending_bundles");
    custody_bundles_ = new BundleList("custody_bundles");

    contactmgr_ = new ContactManager();
    fragmentmgr_ = new FragmentManager();
    reg_table_ = new RegistrationTable();

    router_ = 0;    
}

/**
 * Virtual initialization function. Overridden in the simulator by
 * the Node class. 
 */
void
BundleDaemon::do_init()
{
    actions_ = new BundleActions();
    eventq_ = new oasys::MsgQueue<BundleEvent*>;
}

/**
 * Dispatches to the virtual post_event implementation.
 */
void
BundleDaemon::post(BundleEvent* event)
{
    instance_->post_event(event);
}

/**
 * Virtual post function, overridden in the simulator to use the
 * modified event queue.
 */
void
BundleDaemon::post_event(BundleEvent* event)
{
    log_debug("posting event with type %s", event->type_str());
    eventq_->push(event);
}

/**
 * Format the given StringBuffer with current routing info.
 */
void
BundleDaemon::get_routing_state(oasys::StringBuffer* buf)
{
    router_->get_routing_state(buf);
    contactmgr_->dump(buf);
}

/**
 * Format the given StringBuffer with the current statistics value.
 */
void
BundleDaemon::get_statistics(oasys::StringBuffer* buf)
{
    buf->appendf("%u pending -- "
                 "%u received -- "
                 "%u locally delivered -- "
                 "%u transmitted -- "
                 "%u expired",
                 (u_int)pending_bundles()->size(),
                 bundles_received_,
                 bundles_delivered_,
                 bundles_transmitted_,
                 bundles_expired_);
}


void
BundleDaemon::generate_status_report(Bundle* bundle, status_report_flag_t flag)
{
    log_debug("generating return receipt status report");
        
    BundleStatusReport* report;
        
    report = new BundleStatusReport(bundle, router_->local_tuple());
    report->set_status(flag);
    report->generate_payload();
    
    BundleReceivedEvent e(report, EVENTSRC_ADMIN,
                          report->payload_.length());
    handle_event(&e);
}


void
BundleDaemon::deliver_to_registration(Bundle* bundle,
                                      Registration* registration)
{
    log_debug("delivering bundle *%p to registration %s",
              bundle, registration->endpoint().c_str());
    
    registration->consume_bundle(bundle);
    ++bundles_delivered_;
    
    // deliver the return receipt status report if requested
    if (bundle->return_rcpt_) {
        generate_status_report(bundle, BundleProtocol::STATUS_DELIVERED);
    }
}

/**
 * Deliver the bundle to any matching registrations.
 */
void
BundleDaemon::check_registrations(Bundle* bundle)
{
    log_debug("checking for matching registrations for bundle *%p", bundle);

    RegistrationList matches;
    RegistrationList::iterator iter;

    reg_table_->get_matching(bundle->dest_, &matches);

    // tells routers that this Bundle has been taken care of by the daemon already
    if(matches.size() > 0) 
        bundle->owner_ = "daemon";

    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        Registration* registration = *iter;
        deliver_to_registration(bundle, registration);
    }
}

void
BundleDaemon::handle_bundle_received(BundleReceivedEvent* event)
{
    Bundle* bundle = event->bundleref_.bundle();
    size_t payload_len = bundle->payload_.length();
    
    // if debug logging is enabled, dump out a verbose printing of the
    // bundle, including all options, otherwise, a more terse log
    if (log_enabled(oasys::LOG_DEBUG)) {
        oasys::StringBuffer buf;
        bundle->format_verbose(&buf);
        log_debug("BUNDLE_RECEIVED: (%u bytes recvd)\n%s",
                  (u_int)event->bytes_received_, buf.c_str());
    } else {
        log_info("BUNDLE_RECEIVED *%p (%u bytes recvd)",
                 bundle, (u_int)event->bytes_received_);
    }

    // XXX/demmer generate the bundle received status report

    /*
     * Check if the bundle isn't complete. If so, do reactive
     * fragmentation.
     */
    if (event->bytes_received_ != bundle->payload_.length()) {
        log_debug("partial bundle, making reactive fragment of %u bytes",
                  (u_int)event->bytes_received_);

        fragmentmgr_->convert_to_fragment(bundle, event->bytes_received_);
    }

    /*
     * Check for inbound proactive fragmentation. If it returns true,
     * then the one big bundle has been replaced with a bundle of
     * small ones, each of which has an individual bundle arrival
     * event, so we ignore this one.
     *
     * XXX/demmer rethink this
     */
    int nfrags =
        fragmentmgr_->proactively_fragment(bundle, proactive_frag_threshold_);
    if (nfrags > 0) {
        log_debug("proactive fragmentation: %u byte bundle => %d fragments",
                  (u_int)payload_len, nfrags);
        return;
    }

    // add the bundle to the master pending queue and the data store
    // (unless the bundle was just reread from the data store)
    add_to_pending(bundle, (event->source_ != EVENTSRC_STORE));
    
    // deliver the bundle to any local registrations that it matches
    check_registrations(bundle);

    // bounce out so the configured routers can do something further
    // with the bundle in response to the event.
}

void
BundleDaemon::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    /**
     * The bundle was delivered to either a next-hop contact or a
     * registration.
     */
    Bundle* bundle = event->bundleref_.bundle();

    log_info("BUNDLE_TRANSMITTED id:%d (%u bytes) %s -> %s",
             bundle->bundleid_, (u_int)event->bytes_sent_,
             event->acked_ ? "ACKED" : "UNACKED",
             event->consumer_->dest_str());

    /*
     * Check for reactive fragmentation. The unsent portion (if any)
     * will show up as a new bundle received event.
     */
    fragmentmgr_->reactively_fragment(bundle, event->bytes_sent_);

    // XXX/demmer generate the bundle forwarded status report

    /*
     * Check with the various routers whether or not the bundle can be
     * deleted.
     */
    // XXX/demmer implement me
    delete_from_pending(bundle);
}

void
BundleDaemon::handle_bundle_expired(BundleExpiredEvent* event)
{
    Bundle* bundle = event->bundleref_.bundle();
    
    log_info("BUNDLE_EXPIRED *%p", bundle);

    delete_from_pending(bundle);

    // XXX/demmer need to notify if we have custody...

    // fall through to notify the routers
}

/**
 * When a new application registration arrives, add it to the
 * registration table and then walk the pending list to see if there
 * are any bundles that need delivery to the given registration.
 */
void
BundleDaemon::handle_registration_added(RegistrationAddedEvent* event)
{
    Registration* registration = event->registration_;
    log_info("REGISTRATION_ADDED %s", registration->endpoint().c_str());

    oasys::ScopeLock l(pending_bundles_->lock());

    BundleList::iterator iter;
    for (iter = pending_bundles_->begin();
         iter != pending_bundles_->end();
         ++iter)
    {
        Bundle* bundle = *iter;

        if (registration->endpoint().match(bundle->dest_))
        {
            deliver_to_registration(bundle, registration);
        }
    }
}

/**
 * Default event handler when a new link is available.
 */
void
BundleDaemon::handle_link_available(LinkAvailableEvent* event)
{
    Link* link = event->link_;
    ASSERT(link->isavailable());

    log_info("LINK_AVAILABLE *%p", link);
}

/**
 * Default event handler when a link is unavailable
 */
void
BundleDaemon::handle_link_unavailable(LinkUnavailableEvent* event)
{
    Link* link = event->link_;
    ASSERT(!link->isavailable());
    
    log_info("LINK UNAVAILABLE *%p", link);
}

void
BundleDaemon::handle_link_state_change_request(LinkStateChangeRequest* request)
{
    Link* link = request->link_;
    Link::state_t new_state = request->state_;
    ContactDownEvent::reason_t reason = request->reason_;
    
    log_info("LINK_STATE_CHANGE_REQUEST *%p [%s -> %s] (%s)", link,
             Link::state_to_str(link->state()),
             Link::state_to_str(new_state),
             ContactDownEvent::reason_to_str(reason));

    switch(new_state) {
    case Link::UNAVAILABLE:
        if (link->state() != Link::AVAILABLE) {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "tried to set state UNAVAILABLE in state %s",
                    link, Link::state_to_str(link->state()));
            return;
        }
        link->set_state(new_state);
        post(new LinkUnavailableEvent(link));
        break;

    case Link::AVAILABLE:
        if (link->state() != Link::UNAVAILABLE) {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "tried to set state AVAILABLE in state %s",
                    link, Link::state_to_str(link->state()));
            return;
        }
        link->set_state(new_state);
        post(new LinkAvailableEvent(link));
        break;
        
    case Link::BUSY:
        log_err("LINK_STATE_CHANGE_REQUEST can't be used for state %s",
                Link::state_to_str(new_state));
        break;
        
    case Link::OPENING:
    case Link::OPEN:
        actions_->open_link(link);
        break;


    case Link::CLOSING:
        if (link->isopen()) {
            Contact* contact = link->contact();
            ASSERT(contact);

            // note that the link is being closed
            link->set_state(Link::CLOSING);
        
            // immediately handle (don't post) a new ContactDownEvent to
            // inform the routers that this link is going away
            ContactDownEvent e(link->contact(), reason);
            handle_event(&e);

        } else {
            // The only case where we should get this event when the link
            // is not actually open is if it's in the process of being
            // opened but the CL can't actually open it. Check this and
            // fall through to close the link and deliver a
            // LinkUnavailableEvent
            if (! link->isopening()) {
                log_err("LINK_CLOSE_REQUEST *%p (%s) in unexpected state %s",
                        link, ContactDownEvent::reason_to_str(reason),
                        link->state_to_str(link->state()));
            }

            // note that the link is being closed
            link->set_state(Link::CLOSING);
        }
    
        // now actually close the link
        link->close();

        // now, based on the reason code, update the link availability and
        // set state accordingly
        if (reason == ContactDownEvent::IDLE) {
            link->set_state(Link::AVAILABLE);
        } else {
            link->set_state(Link::UNAVAILABLE);
            post(new LinkUnavailableEvent(link));
        }

        break;

    default:
        PANIC("unhandled state %s", Link::state_to_str(new_state));
    }
}
  
void
BundleDaemon::handle_contact_up(ContactUpEvent* event)
{
    Contact* contact = event->contact_;
    log_info("CONTACT_UP *%p", contact);
    
    Link* link = event->contact_->link();
    link->set_state(Link::OPEN);
}

void
BundleDaemon::handle_contact_down(ContactDownEvent* event)
{
    Contact* contact = event->contact_;
    log_info("CONTACT_DOWN *%p", contact);
}

/**
 * Default event handler when reassembly is completed. For each
 * bundle on the list, check the pending count to see if the
 * fragment can be deleted.
 */
void
BundleDaemon::handle_reassembly_completed(ReassemblyCompletedEvent* event)
{
    log_info("REASSEMBLY_COMPLETED bundle id %d",
             event->bundle_.bundle()->bundleid_);
    
    Bundle* bundle;
    while ((bundle = event->fragments_.pop_front()) != NULL) {
        // remove the fragment from the pending list
        // XXX/demmer fix me re deleting bundles
        delete_from_pending(bundle);
        bundle->del_ref("bundle_list", event->fragments_.name().c_str());
    }

    // post a new event for the newly reassembled event
    post(new BundleReceivedEvent(bundle, EVENTSRC_FRAGMENTATION,
                                 bundle->payload_.length()));
}

/**
 * Add the bundle to the pending list and persistent store, and
 * set up the expiration timer for it.
 */
void
BundleDaemon::add_to_pending(Bundle* bundle, bool add_to_store)
{
    log_debug("adding bundle *%p to pending list", bundle);
    
    pending_bundles_->push_back(bundle);
    
    if (add_to_store) {
        actions_->store_add(bundle);
    }

    // schedule the bundle expiration timer
    log_debug("scheduling expiration timer for bundle id %d", bundle->bundleid_);
    struct timeval expiration_time = bundle->creation_ts_;
    expiration_time.tv_sec += bundle->expiration_;
    bundle->expiration_timer_ = new ExpirationTimer(bundle);
    bundle->expiration_timer_->schedule_at(&expiration_time);

    // log a warning if the bundle doesn't have any lifetime
    if (bundle->expiration_ == 0) {
        log_warn("bundle arrived with zero expiration time");
    }
}

/**
 * Delete the given bundle from the pending list.
 */
void
BundleDaemon::delete_from_pending(Bundle* bundle)
{
    log_debug("removing bundle *%p from pending list and persistent store",
              bundle);

    actions_->store_del(bundle);

    if (! pending_bundles_->erase(bundle)) {
        log_err("unexpected error removing bundle from pending list");
    }

    // cancel the expiration timer
    if (bundle->expiration_timer_) {
        log_debug("cancelling expiration timer for bundle id %d",
                  bundle->bundleid_);
        bundle->expiration_timer_->cancel();
        bundle->expiration_timer_ = NULL;
    }
}

/**
 * Update statistics based on the event arrival.
 */
void
BundleDaemon::update_statistics(BundleEvent* event)
{
    switch(event->type_) {

    case BUNDLE_RECEIVED:
        bundles_received_++;
        break;
        
    case BUNDLE_TRANSMITTED:
        // XXX/demmer wonky
        if (! ((BundleTransmittedEvent*)event)->consumer_->is_local()) {
            bundles_transmitted_++;
        }
        break;

    case BUNDLE_EXPIRED:
        bundles_expired_++;
        break;
        
    default:
        break;
    }
}

/**
 * Event handler override for the daemon. First dispatches to the
 * local event handlers, then to the list of attached routers.
 */
void
BundleDaemon::handle_event(BundleEvent* event)
{
    update_statistics(event); // XXX/demmer remove the fn and move
                              // into individual handlers
     
    dispatch_event(event);
    
    if (! event->daemon_only_) {
        // dispatch the event to the router(s)
        router_->handle_event(event);
        
        // XXX/demmer dispatch to contact mgr?
    }
}

/**
 * The main run loop.
 */
void
BundleDaemon::run()
{
    BundleEvent* event;
    
    while (1) {
        // grab an event off the queue, blocking until we get one
        event = eventq_->pop_blocking();
        ASSERT(event);

        // handle the event
        handle_event(event);
        
        // clean up the event
        delete event;
    }
}


} // namespace dtn

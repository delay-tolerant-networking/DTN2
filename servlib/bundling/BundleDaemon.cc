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
#include "ExpirationTimer.h"
#include "FragmentManager.h"
#include "contacts/Contact.h"
#include "contacts/ContactManager.h"
#include "reg/Registration.h"
#include "reg/RegistrationTable.h"
#include "routing/BundleRouter.h"

namespace dtn {

BundleDaemon* BundleDaemon::instance_ = NULL;
BundleDaemon::Params BundleDaemon::params_;

BundleDaemon::BundleDaemon()
    : BundleEventHandler("/bundle/daemon"),
      Thread("BundleDaemon", CREATE_JOINABLE)
{
    // default local eid
    // XXX/demmer fixme
    local_eid_.assign("dtn:localhost");

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

    app_shutdown_proc_ = NULL;
    app_shutdown_data_ = NULL;
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
 * Queues the event for processing by the daemon thread.
 */
void
BundleDaemon::post(BundleEvent* event)
{
    instance_->post_event(event);
}

/**
 * Post the given event and wait for it to be processed by the
 * daemon thread.
 */
void
BundleDaemon::post_and_wait(BundleEvent* event, oasys::Notifier* notifier)
{
    ASSERT(event->processed_notifier_ == NULL);
    event->processed_notifier_ = notifier;
    post(event);
    notifier->wait();
}

/**
 * Virtual post function, overridden in the simulator to use the
 * modified event queue.
 */
void
BundleDaemon::post_event(BundleEvent* event)
{
    log_debug("posting event (%p) with type %s", event, event->type_str());
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
BundleDaemon::generate_status_report(Bundle* bundle,
                                     status_report_flag_t flag,
                                     status_report_reason_t reason)
{
    log_debug("generating return receipt status report, "
              "flag = 0x%x, reason = 0x%x", flag, reason);
        
    BundleStatusReport* report;
        
    report = new BundleStatusReport(bundle, local_eid_, flag, reason);
    
    BundleReceivedEvent e(report, EVENTSRC_ADMIN,
                          report->payload_.length());
    handle_event(&e);
}


void
BundleDaemon::deliver_to_registration(Bundle* bundle,
                                      Registration* registration)
{
    log_debug("delivering bundle *%p to registration %d (%s)",
              bundle, registration->regid(),
              registration->endpoint().c_str());
    
    registration->consume_bundle(bundle);
}

/**
 * Deliver the bundle to any matching registrations.
 */
void
BundleDaemon::check_registrations(Bundle* bundle)
{
    int num;
    log_debug("checking for matching registrations for bundle *%p", bundle);

    RegistrationList matches;
    RegistrationList::iterator iter;

    num = reg_table_->get_matching(bundle->dest_, &matches);

    // tells routers that this Bundle has been taken care of
    // by the daemon already
    if (num > 0) {
        bundle->owner_ = "daemon";
    }

    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        Registration* registration = *iter;
        deliver_to_registration(bundle, registration);
    }
}

void
BundleDaemon::handle_bundle_received(BundleReceivedEvent* event)
{
    Bundle* bundle = event->bundleref_.object();
    size_t payload_len = bundle->payload_.length();
    
    // if debug logging is enabled, dump out a verbose printing of the
    // bundle, including all options, otherwise, a more terse log
    if (log_enabled(oasys::LOG_DEBUG)) {
        oasys::StaticStringBuffer<1024> buf;
        buf.appendf("BUNDLE_RECEIVED: (%u bytes recvd)\n",
                    (u_int)event->bytes_received_);
        bundle->format_verbose(&buf);
        log_multiline(oasys::LOG_DEBUG, buf.c_str());
    } else {
        log_info("BUNDLE_RECEIVED *%p (%u bytes recvd)",
                 bundle, (u_int)event->bytes_received_);
    }
    
    // log a warning if the bundle doesn't have any expiration time or
    // has a creation time in the past. in either case, we proceed as
    // normal, though we know that the expiration timer will
    // immediately fire
    if (bundle->expiration_ == 0) {
        log_warn("bundle id %d arrived with zero expiration time",
                 bundle->bundleid_);
    }

    struct timeval now;
    gettimeofday(&now, 0);
    if (TIMEVAL_GT(bundle->creation_ts_, now)) {
        log_warn("bundle id %d arrived with creation time in the future "
                 "(%u.%u > %u.%u)",
                 bundle->bundleid_,
                 (u_int)bundle->creation_ts_.tv_sec,
                 (u_int)bundle->creation_ts_.tv_usec,
                 (u_int)now.tv_sec, (u_int)now.tv_usec);
    }
        
    if (bundle->receive_rcpt_) {
        generate_status_report(bundle, BundleProtocol::STATUS_RECEIVED);
    }

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
        fragmentmgr_->proactively_fragment(bundle, params_.proactive_frag_threshold_);
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
     * The bundle was delivered to a next-hop contact.
     */
    Bundle* bundle = event->bundleref_.object();

    log_info("BUNDLE_TRANSMITTED id:%d (%u bytes) %s -> %s",
             bundle->bundleid_, (u_int)event->bytes_sent_,
             event->acked_ ? "ACKED" : "UNACKED",
             event->contact_->nexthop());

    /**
     * Update the forwarding log
     */
    bundle->fwdlog_.update(event->contact_->nexthop(), ForwardingEntry::SENT);
                            
    /*
     * Check for reactive fragmentation. The unsent portion (if any)
     * will show up as a new bundle received event.
     */
    fragmentmgr_->reactively_fragment(bundle, event->bytes_sent_);

    if (bundle->forward_rcpt_) {
        // XXX/matt todo: change to generate with a reason code of
        // "forwarded over unidirectional link" if the bundle has the
        // retention constraint "custody accepted" and all of the
        // nodes in the minimum reception group of the endpoint
        // selected for forwarding are known to be unable to send
        // bundles back to this node
        generate_status_report(bundle, BundleProtocol::STATUS_FORWARDED);
    }

    /*
     * If there are no more mappings for the bundle (except for the
     * pending list), and we're configured for early deletion, then
     * remove the bundle from the pending list. Then when all refs to
     * the bundle are removed, it will be timed out.
     *
     * This allows a router (or the custody system) to maintain a
     * retention constraint by adding a mapping or just adding a
     * reference.
     */
    if (params_.early_deletion_ && (bundle->num_mappings() == 1)) {
        // XXX/matt if we delete a bundle just because there are no
        // more mappings, and not because it expired, then we don't
        // want to generate a status report, so pass the
        // REASON_NO_ADDTL_INFO reason code
        delete_from_pending(bundle, BundleProtocol::REASON_NO_ADDTL_INFO);
    }
}

void
BundleDaemon::handle_bundle_delivered(BundleDeliveredEvent* event)
{
    /**
     * The bundle was delivered to a registration.
     */
    Bundle* bundle = event->bundleref_.object();

    log_info("BUNDLE_DELIVERED id:%d (%u bytes) -> regid %d (%s)",
             bundle->bundleid_, bundle->payload_.length(),
             event->registration_->regid(),
             event->registration_->endpoint().c_str());

    if (bundle->delivery_rcpt_) {
        generate_status_report(bundle, BundleProtocol::STATUS_DELIVERED);
    }

    /*
     * If there are no more mappings for the bundle (except for the
     * pending list), and we're configured for early deletion, then
     * remove the bundle from the pending list. Then when all refs to
     * the bundle are removed, it will be timed out.
     *
     * This allows a router (or the custody system) to maintain a
     * retention constraint by adding a mapping or just adding a
     * reference.
     */
    if (params_.early_deletion_ && (bundle->num_mappings() == 1)) {
        // XXX/matt if we delete a bundle just because there are no
        // more mappings, and not because it expired, then we don't
        // want to generate a status report, so pass the
        // REASON_NO_ADDTL_INFO reason code
        delete_from_pending(bundle, BundleProtocol::REASON_NO_ADDTL_INFO);
    }
}

void
BundleDaemon::handle_bundle_expired(BundleExpiredEvent* event)
{
    Bundle* bundle = event->bundleref_.object();
    oasys::ScopeLock l(&bundle->lock_, "BundleDaemon::handle_bundle_expired");

    log_info("BUNDLE_EXPIRED *%p", bundle);

    ASSERT(bundle->expiration_timer_ == NULL);

    // XXX/demmer need to notify if we have custody...

    // check that the bundle is on the pending list and then remove it
    // if it is. if it's not, then there was a race between the
    // expiration timer and a prior call to delete_from_pending. check
    // that the bundle isn't on any other lists (which it shouldn't
    // be), and return
    if (pending_bundles_->contains(bundle)) {
        delete_from_pending(bundle, BundleProtocol::REASON_LIFETIME_EXPIRED);

    } else {
        if (bundle->num_mappings() == 0) {
            log_debug("expired bundle *%p already removed from pending "
                      "list and all other lists", bundle);
            return;

        } else {
            log_err("expired bundle *%p not on pending list but still "
                    "queued on other lists!!", bundle);

            // fall through to remove other mappings
        }
    }
    
    // now remove all other mappings as well
    bool deleted;
    BundleList* bundle_list;
    while (bundle->num_mappings() != 0) {
        bundle_list = *bundle->mappings_begin();
        ASSERT(bundle_list != NULL);
        deleted = bundle_list->erase(bundle);
        ASSERT(deleted);
    }

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
    log_info("REGISTRATION_ADDED %d %s",
             registration->regid(), registration->endpoint().c_str());

    if (!reg_table_->add(registration)) {
        log_err("error adding registration %d to table",
                registration->regid());
    }
    
    oasys::ScopeLock l(pending_bundles_->lock(), 
                       "BundleDaemon::handle_registration_added");
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

void
BundleDaemon::handle_registration_removed(RegistrationRemovedEvent* event)
{
    Registration* registration = event->registration_;
    log_info("REGISTRATION_REMOVED %d %s",
             registration->regid(), registration->endpoint().c_str());
    
    if (!reg_table_->del(registration->regid())) {
        log_err("error removing registration %d from table",
                registration->regid());
    }

    delete registration;
}

void
BundleDaemon::handle_registration_expired(RegistrationExpiredEvent* event)
{
    
    Registration* registration = reg_table_->get(event->regid_);
    if (registration == NULL) {
        log_warn("REGISTRATION_EXPIRED -- dead regid %d", event->regid_);
        return;
    }

    registration->set_expired(true);
    
    if (registration->active()) {
        // if the registration is currently active (i.e. has a
        // binding), we wait for the binding to clear, which will then
        // clean up the registration
        log_info("REGISTRATION_EXPIRED %d -- deferred until binding clears",
                 event->regid_);
    } else {
        // otherwise remove the registration from the table
        log_info("REGISTRATION_EXPIRED %d", event->regid_);
        reg_table_->del(registration->regid());
        delete registration;
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
    ContactEvent::reason_t reason = request->reason_;
    
    log_info("LINK_STATE_CHANGE_REQUEST *%p [%s -> %s] (%s)", link,
             Link::state_to_str(link->state()),
             Link::state_to_str(new_state),
             ContactEvent::reason_to_str(reason));

    switch(new_state) {
    case Link::UNAVAILABLE:
        if (link->state() != Link::AVAILABLE) {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "tried to set state UNAVAILABLE in state %s",
                    link, Link::state_to_str(link->state()));
            return;
        }
        link->set_state(new_state);
        post(new LinkUnavailableEvent(link, reason));
        break;

    case Link::AVAILABLE:
        if (link->state() == Link::UNAVAILABLE) {
            link->set_state(Link::AVAILABLE);

        } else if (link->state() == Link::BUSY) {
            link->set_state(Link::OPEN);

        } else if (link->state() == Link::OPEN) {
            // a CL might send multiple requests to go from
            // BUSY->OPEN, so we can safely ignore this
            
        } else {
            log_err("LINK_STATE_CHANGE_REQUEST *%p: "
                    "tried to set state AVAILABLE in state %s",
                    link, Link::state_to_str(link->state()));
            return;
        }
        
        post(new LinkAvailableEvent(link, reason));
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
        if (link->isclosing()) {
            log_warn("link close request for *%p (%s) already in closing state",
                     link, ContactEvent::reason_to_str(reason));
            break;
        }
        
        if (link->isopen()) {
            Contact* contact = link->contact();
            ASSERT(contact);

            // note that the link is being closed
            link->set_state(Link::CLOSING);
        
        } else {
            // The only case where we should get this event when the link
            // is not actually open is if it's in the process of being
            // opened but the CL can't actually open it. Check this and
            // fall through to close the link and deliver a
            // LinkUnavailableEvent
            if (! link->isopening()) {
                log_err("LINK_CLOSE_REQUEST *%p (%s) in unexpected state %s",
                        link, ContactEvent::reason_to_str(reason),
                        link->state_to_str(link->state()));
            }

            // note that the link is being closed
            link->set_state(Link::CLOSING);
        }
    
        // if this request is user generated, we need to inform the
        // routers that the contact is going down
        if (reason == ContactEvent::USER) {
            ContactDownEvent e(link->contact(), reason);
            handle_event(&e);
        }
        
        // now actually close the link
        link->close();

        // now, based on the reason code, update the link availability
        // and set state accordingly
        if (reason == ContactEvent::IDLE) {
            link->set_state(Link::AVAILABLE);
        } else {
            link->set_state(Link::UNAVAILABLE);
            post(new LinkUnavailableEvent(link, reason));
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
    Link* link = event->contact_->link();
    ContactEvent::reason_t reason = event->reason_;
    
    log_info("CONTACT_DOWN *%p (%s)",
             contact, ContactEvent::reason_to_str(reason));
    
    // based on the reason code, update the link availability
    // and set state accordingly
    if (reason == ContactEvent::IDLE) {
        post(new LinkStateChangeRequest(link, Link::CLOSING, reason));
    } else {
        link->set_state(Link::UNAVAILABLE);
        post(new LinkUnavailableEvent(link, reason));
    }
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
             event->bundle_->bundleid_);
    
    BundleRef ref("BundleDaemon::handle_reassembly_completed temporary");
    while ((ref = event->fragments_.pop_front()) != NULL) {
        // remove the fragment from the pending list
        delete_from_pending(ref.object(),
                            BundleProtocol::REASON_NO_ADDTL_INFO);
    }

    // post a new event for the newly reassembled event
    post(new BundleReceivedEvent(ref.object(), EVENTSRC_FRAGMENTATION,
                                 ref->payload_.length()));
}

void
BundleDaemon::handle_shutdown_request(ShutdownRequest* request)
{
    log_notice("Received shutdown request");

    oasys::ScopeLock l(contactmgr_->lock(), "BundleDaemon::handle_shutdown");
    
    const LinkSet* links = contactmgr_->links();
    LinkSet::const_iterator iter;
    Link* link = NULL;

    // close any open links
    for (iter = links->begin(); iter != links->end(); ++iter)
    {
        link = *iter;
        if (link->isopen()) {
            log_debug("Shutdown: closing link *%p\n", link);
            if ((link->state() != Link::CLOSING)) {
                link->set_state(Link::CLOSING);
            }
            link->close();
        }
    }

    // XXX/todo: cleanly sync the various data stores

    if (app_shutdown_proc_) {
        (*app_shutdown_proc_)(app_shutdown_data_);
    }

    // signal to the main loop to bail
    set_should_stop();
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
    struct timeval expiration_time = bundle->creation_ts_;
    expiration_time.tv_sec += bundle->expiration_;
    bundle->expiration_timer_ = new ExpirationTimer(bundle);
    bundle->expiration_timer_->schedule_at(&expiration_time);

    struct timeval now;
    gettimeofday(&now, 0);
    long int when = TIMEVAL_DIFF_MSEC(expiration_time, now);

    if (when > 0) {
        log_debug("scheduling expiration for bundle id %d at %u.%u (in %lu msec)",
                  bundle->bundleid_,
                  (u_int)expiration_time.tv_sec, (u_int)expiration_time.tv_usec, when);
    } else {
        log_warn("scheduling IMMEDIATE expiration for bundle id %d: "
                 "[expiration %u, creation time %u.%u, now %u.%u]",
                 bundle->bundleid_,
                 (u_int)bundle->creation_ts_.tv_sec,
                 (u_int)bundle->creation_ts_.tv_usec,
                 bundle->expiration_, (u_int)now.tv_sec, (u_int)now.tv_usec);
    }
}

/**
 * Delete the given bundle from the pending list. If the reason code
 * is REASON_NO_ADDTL_INFO, we will never send a BundleStatusReport
 * regardless of whether deletion_rcpt_ is set
 */
void
BundleDaemon::delete_from_pending(Bundle* bundle,
                                  status_report_reason_t reason)
{
    oasys::ScopeLock l(&bundle->lock_, "BundleDaemon::delete_from_pending");
    
    log_debug("removing bundle *%p from pending list", bundle);

    // first try to cancel the expiration timer if it's still
    // around
    if (bundle->expiration_timer_) {
        log_debug("cancelling expiration timer for bundle id %d",
                  bundle->bundleid_);
        
        bool cancelled = bundle->expiration_timer_->cancel();
        if (cancelled) {
            bundle->expiration_timer_->bundleref_.release();
            bundle->expiration_timer_ = NULL;
        } else {
            // if the timer is no longer in the pending queue (i.e. it
            // can't be cancelled), then we just hit a race where the
            // timer is about to fire, which is ok since
            // handle_bundle_expired takes care of this correctly
            bundle->expiration_timer_ = NULL;
        }
    }
    
    if (pending_bundles_->erase(bundle)) {
        if (bundle->deletion_rcpt_ &&
            (reason != BundleProtocol::REASON_NO_ADDTL_INFO))
        {
            generate_status_report(bundle,
                                   BundleProtocol::STATUS_DELETED,
                                   reason);
        }
    } else {
        log_err("unexpected error removing bundle from pending list");
    }
}

/**
 * The last step in the lifetime of a bundle occurs when there are no
 * more references to it and we get this event.
 */
void
BundleDaemon::handle_bundle_free(BundleFreeEvent* event)
{
    Bundle* bundle = event->bundle_;
    event->bundle_ = NULL;
    ASSERT(bundle->refcount() == 0);
    
    bundle->lock_.lock("BundleDaemon::handle_bundle_free");
    
    actions_->store_del(bundle);
    
    delete bundle;
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
        bundles_transmitted_++;
        break;

    case BUNDLE_DELIVERED:
        bundles_delivered_++;
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
        // dispatch the event to the router(s) and the contact manager
        router_->handle_event(event);
        contactmgr_->handle_event(event);
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
        if (should_stop()) {
            break;
        }
        
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

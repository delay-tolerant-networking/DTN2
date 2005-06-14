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
    
    registration->consume_bundle(bundle, NULL);
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
    
    log_info("BUNDLE_RECEIVED id:%d (%u of %u bytes)",
             bundle->bundleid_, (u_int)event->bytes_received_,
             (u_int)payload_len);

    log_debug("bundle %d source    %s", bundle->bundleid_, bundle->source_.c_str());
    log_debug("bundle %d dest      %s", bundle->bundleid_, bundle->dest_.c_str());
    log_debug("bundle %d replyto   %s", bundle->bundleid_, bundle->replyto_.c_str());
    log_debug("bundle %d custodian %s", bundle->bundleid_, bundle->custodian_.c_str());

    // XXX/demmer generate the bundle received status report

    /*
     * Check if the bundle isn't complete. If so, do reactive
     * fragmentation.
     */
    if (event->bytes_received_ != bundle->payload_.length()) {
        log_debug("partial bundle, making reactive fragment of %u bytes",
                  (u_int)event->bytes_received_);
        BundleDaemon::instance()->fragmentmgr()->
            convert_to_fragment(bundle, event->bytes_received_);
    }

    /*
     * Check for proactive fragmentation. If it returns true, then the
     * one big bundle has been replaced with a bundle of small ones,
     * each of which generates individual bundle events.
     */
    int nfrags =
        fragmentmgr_->proactively_fragment(bundle, proactive_frag_threshold_);
    if (nfrags > 0) {
        log_debug("proactive fragmentation: %u byte bundle => %d fragments",
                  payload_len, nfrags);
        return;
    }

    // add the bundle to the master pending queue
    pending_bundles_->push_back(bundle, NULL);

    // add the bundle to the store (unless we're reloading it in which
    // case the bundle just came from there)
    if (event->source_ != EVENTSRC_STORE) {
        actions_->store_add(bundle);
    }

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
    log_info("LINK_AVAILABLE *%p", event->link_);
    
    Link* link = event->link_;
    ASSERT(link->isavailable());

    // If something is queued on the link open it
    if (link->size() > 0) {
        actions_->open_link(link);
    }
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
    
    // Close the contact on the link if link is open
    // i.e Transfer messages queued on the contact to the link
    // Above two objectives can be achieved by posting the event
    // that contact is down for this link, which would cleanup
    // the state of queues
    if (link->isopen()) 
        BundleDaemon::post(new ContactDownEvent(link->contact()));
}

/**
 * Default event handler when a new contact is available.
 */
void
BundleDaemon::handle_contact_up(ContactUpEvent* event)
{
    Contact* contact = event->contact_;
    Link* link = contact->link();
    log_info("CONTACT_UP *%p", contact);

    // ASSERT(contact->bundle_list()->size() == 0);
    // Above assertion may not hold because there may be bundles
    // which are inserted between contact is open and contact_up event
    // is  seen by the router. Such bundles will go to contact queue
    // because the contact is theoretically open although router has not
    // seen contact_up event so far
    
    // Move any messages from link queue to contact queue
    actions_->move_contents(link, contact);
}

/**
 * Default event handler when a contact is down
 * This event is posted  by convergence layer
 * on detecting a failure or by bundle router on detecting
 * that link is unavailable.
 * This function moves messages from contact queue to link
 * queue
 */
void
BundleDaemon::handle_contact_down(ContactDownEvent* event)
{

    Contact* contact = event->contact_;
    Link* link = contact->link();
    log_info("CONTACT_DOWN *%p: re-routing %d queued bundles",
             contact, (u_int)contact->bundle_list()->size());

    // note that there is a chance we didn't get a contact_up (perhaps
    // the initial connection attept failed), so don't make assertions
    // about the state of the link's bundle list
    // XXX/demmer revisit this
    actions_->move_contents(contact, link);
    
    // Close the contact
    actions_->close_link(link);

    // Check is link is suppoed to be available and if yes
    // try to reopen the link

    if ((link->size() > 0) && link->isavailable()) {
        actions_->open_link(link);
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
             event->bundle_.bundle()->bundleid_);
    
    Bundle* bundle;
    while ((bundle = event->fragments_.pop_front()) != NULL) {
        // XXX/demmer fixme
        delete_from_pending(bundle);
        bundle->del_ref("bundle_list", event->fragments_.name().c_str());
    }

    // add the newly reassembled bundle to the pending list and the
    // data store
    bundle = event->bundle_.bundle();
    pending_bundles_->push_back(bundle, NULL);
    actions_->store_add(bundle);

    // XXX/demmer need a new bundle arrival event
}


/**
 * Delete the given bundle from the pending list (assumes the
 * pending count is zero).
 */
void
BundleDaemon::delete_from_pending(Bundle* bundle)
{
    log_debug("removing bundle *%p from pending list", bundle);
    
    actions_->store_del(bundle);

    BundleMapping* mapping = bundle->get_mapping(pending_bundles_);
    ASSERT(mapping);
    pending_bundles_->erase(mapping->position_, NULL);
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

void
BundleDaemon::handle_event(BundleEvent* event)
{
    update_statistics(event);
    dispatch_event(event);
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
        
        // dispatch the event to the router(s)
        router_->handle_event(event);

        // clean up the event
        delete event;
    }
}


} // namespace dtn

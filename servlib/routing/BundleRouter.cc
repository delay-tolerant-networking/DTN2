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

#include <stdlib.h>

#include "BundleRouter.h"
#include "RouteTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleStatusReport.h"
#include "bundling/Contact.h"
#include "bundling/FragmentManager.h"
#include "reg/Registration.h"

#include "StaticBundleRouter.h"
#include "FloodBundleRouter.h"

namespace dtn {

BundleRouter::config_t BundleRouter::Config;
size_t BundleRouter::proactive_frag_threshold_;


/*
 * Note by sushant:
 * Reasons to open a link (which is closed) in the default router implementation:
 * (note, link can be opened only if link is available)
 * 1. Link available event and Link queue has > 0 messages
 * 2. Contact_down event and link is available and queue has > 0 messages
 * 3. New message on link queue and link is available
 *
 * TODO: Peer routing semantics
 * When Link Available event arrives transfer message from peer queue
 * to Link queue
 * Also, if new message on peer queue and atleast one link is available
 */

/**
 * Factory method to create the correct subclass of BundleRouter
 * for the registered algorithm type.
 */
BundleRouter*
BundleRouter::create_router(const char* type)
{
    if (!strcmp(type, "static")) {
        return new StaticBundleRouter();
    }
    else if (!strcmp(type, "flood")) {
        return new FloodBundleRouter();
    }
    else {
        PANIC("unknown type %s for router", type);
    }
}

/**
 * Constructor.
 */
BundleRouter::BundleRouter()
    : Logger("/route"),
      local_regions_(Config.local_regions_),
      local_tuple_(Config.local_tuple_)
{
    if (local_tuple_.length() == 0) {
        local_tuple_.assign("bundles://internet/host://localhost");
    }
    
    log_debug("router initializing (local_tuple %s)",
              local_tuple_.c_str());
    
    route_table_ = new RouteTable();

    // XXX/demmer maybe change this?
    pending_bundles_ = BundleDaemon::instance()->pending_bundles();
    custody_bundles_ = BundleDaemon::instance()->custody_bundles();
}

/**
 * Destructor
 */
BundleRouter::~BundleRouter()
{
    NOTIMPLEMENTED;
}

/**
 * The monster routing decision function that is called in
 * response to all received events. To cause bundles to be
 * forwarded, this function populates the given action list with
 * the forwarding decisions. The run() method takes the decisions
 * from the active router and passes them to the BundleDaemon.
 *
 * Note that the function is virtual so more complex derived
 * classes can override or augment the default behavior.
 */
void
BundleRouter::handle_event(BundleEvent* e,
                           BundleActions* actions)
{
    log_debug("Router executing event %s",e->type_str());
    switch(e->type_) {

    case BUNDLE_RECEIVED:
        handle_bundle_received((BundleReceivedEvent*)e, actions);
        break;

    case BUNDLE_TRANSMITTED:
        handle_bundle_transmitted((BundleTransmittedEvent*)e, actions);
        break;
        
    case REGISTRATION_ADDED:
        handle_registration_added((RegistrationAddedEvent*)e, actions);
        break;

    case ROUTE_ADD:
        handle_route_add((RouteAddEvent*)e, actions);
        break;

    case CONTACT_UP:
        handle_contact_up((ContactUpEvent*)e, actions);
        break;

    case CONTACT_DOWN:
        handle_contact_down((ContactDownEvent*)e, actions);
        break;

    case LINK_CREATED:
        handle_link_created((LinkCreatedEvent*)e, actions);
        break;

    case LINK_DELETED:
        handle_link_deleted((LinkDeletedEvent*)e, actions);
        break;

    case LINK_AVAILABLE:
        handle_link_available((LinkAvailableEvent*)e, actions);
        break;

    case LINK_UNAVAILABLE:
        handle_link_unavailable((LinkUnavailableEvent*)e, actions);
        break;

    case REASSEMBLY_COMPLETED:
        handle_reassembly_completed((ReassemblyCompletedEvent*)e, actions);
        break;

    default:
        PANIC("unimplemented event type %d", e->type_);
    }

    delete e;
}

/**
 * Default event handler for new bundle arrivals.
 * Queue the bundle on the pending delivery list, and then
 * searches through the route table to find any matching next
 * contacts, filling in the action list with forwarding decisions.
 */
void
BundleRouter::handle_bundle_received(BundleReceivedEvent* event,
                                     BundleActions* actions)
{
    Bundle* bundle = event->bundleref_.bundle();
    
    log_info("BUNDLE_RECEIVED id:%d (%d of %d bytes)",
             bundle->bundleid_, event->bytes_received_,
             bundle->payload_.length());
    log_debug("bundle %d source    %s", bundle->bundleid_, bundle->source_.c_str());
    log_debug("bundle %d dest      %s", bundle->bundleid_, bundle->dest_.c_str());
    log_debug("bundle %d replyto   %s", bundle->bundleid_, bundle->replyto_.c_str());
    log_debug("bundle %d custodian %s", bundle->bundleid_, bundle->custodian_.c_str());

    /*
     * Check if the bundle isn't complete. If so, do reactive
     * fragmentation.
     *
     * XXX/demmer this should maybe be changed to do the reactive
     * fragmentation in the forwarder?
     */
    if (event->bytes_received_ != bundle->payload_.length()) {
        log_debug("partial bundle, making fragment of %d bytes",
                  event->bytes_received_);
        FragmentManager::instance()->
            convert_to_fragment(bundle, event->bytes_received_);
    }

    /*
     * Check for proactive fragmentation
     */
    size_t payload_len = bundle->payload_.length();
    if ((proactive_frag_threshold_ > 0) &&
        (payload_len > proactive_frag_threshold_))
    {
        log_info("proactively fragmenting "
                 "%d byte bundle into %d %d byte fragments",
                 payload_len, payload_len / proactive_frag_threshold_,
                 proactive_frag_threshold_);

        // XXX/demmer move this into the FragmentManager to return a
        // bundlelist, not to do all the nitty-gritty here
        Bundle* fragment;
        
        int todo = payload_len;
        int offset = 0;
        int fraglen = proactive_frag_threshold_;
        while (todo > 0) {
            if ((fraglen + offset) > (int)bundle->payload_.length()) {
                fraglen = bundle->payload_.length() - offset;
            }

            fragment = FragmentManager::instance()->
                       create_fragment(bundle, offset, fraglen);
            ASSERT(fragment);

            pending_bundles_->push_back(fragment, NULL);
            actions->store_add(fragment);
            fwd_to_matching(fragment, actions, false);
            
            offset += fraglen;
            todo -= fraglen;
        }

        bundle->payload_.close_file();
        return;
    }

    pending_bundles_->push_back(bundle, NULL);
    if (event->source_ != EVENTSRC_STORE) {
        // don't add the bundle to the store if it just came from there
        actions->store_add(bundle);
    }
    fwd_to_matching(bundle, actions, true);
}

void
BundleRouter::handle_bundle_transmitted(BundleTransmittedEvent* event,
                                        BundleActions* actions)
{
    /**
     * The bundle was delivered to either a next-hop contact or a
     * registration.
     */
    Bundle* bundle = event->bundleref_.bundle();
    BundleConsumer* consumer = event->consumer_;

    log_info("BUNDLE_TRANSMITTED id:%d (%d bytes) %s -> %s",
             bundle->bundleid_, event->bytes_sent_,
             event->acked_ ? "ACKED" : "UNACKED",
             event->consumer_->dest_str());

    /*
     * If the whole bundle was sent and this is the last destination
     * that needs a copy of the bundle, we can safely remove it from
     * the pending list.
     */
    if (bundle->del_pending() == 0) {
        delete_from_pending(bundle, actions);
    }

    /*
     * Check for reactive fragmentation, potentially splitting off the
     * unsent portion as a new bundle, if necessary.
     */
    size_t payload_len = bundle->payload_.length();

    if (event->bytes_sent_ != payload_len) {
        size_t frag_off = event->bytes_sent_;
        size_t frag_len = payload_len - event->bytes_sent_;

        log_debug("incomplete transmission: creating reactive fragment "
                  "(offset %d len %d/%d)", frag_off, frag_len, payload_len);
        
        Bundle* tail = FragmentManager::instance()->
                       create_fragment(bundle, frag_off, frag_len);

        bundle->payload_.close_file();
        
        // XXX/demmer temp to put it on the head of the contact list
        tail->is_reactive_fragment_ = true;
        
        // treat the new fragment as if it just arrived (sort of). we
        // don't want to store the fragment if no-one will consume it.

        int count = fwd_to_matching(tail, actions, false);

        if (count > 0) {
            pending_bundles_->push_back(tail, NULL);
            actions->store_add(tail);
        }
    }

    /*
     * Check for delivery status notification requests.
     */
    BundleStatusReport* report;
    if (bundle->return_rcpt_ && consumer->is_local()) {
        log_debug("generating return receipt status report");
        report = new BundleStatusReport(bundle, local_tuple_);
        report->set_status_time(BundleProtocol::STATUS_DELIVERED);

        pending_bundles_->push_back(report, NULL);
        actions->store_add(report);
        fwd_to_matching(report, actions, true);
    }
}

/**
 * Default event handler when a new application registration
 * arrives.
 *
 * Adds an entry to the route table for the new registration, then
 * walks the pending list to see if any bundles match the
 * registration.
 */
void
BundleRouter::handle_registration_added(RegistrationAddedEvent* event,
                                        BundleActions* actions)
{
    Registration* registration = event->registration_;
    log_debug("new registration for %s", registration->endpoint().c_str());

    RouteEntry* entry = new RouteEntry(registration->endpoint(),
                                       registration, FORWARD_REASSEMBLE);
    route_table_->add_entry(entry);
    new_next_hop(registration->endpoint(), registration, actions);
}

/**
 * Default event handler when a new link is created
 */
void
BundleRouter::handle_link_created(LinkCreatedEvent* event,
                                  BundleActions* actions)
{
    log_info("LINK_CREATED *%p", event->link_);
}

/**
 * Default event handler when a link is deleted
 */
void
BundleRouter::handle_link_deleted(LinkDeletedEvent* event,
                                  BundleActions* actions)
{
    // Take care of all messages queued for this link
    // Take care of routing entries referring to this
    NOTIMPLEMENTED ; 
}

/**
 * Default event handler when a new link is available.
 */
void
BundleRouter::handle_link_available(LinkAvailableEvent* event,
                                    BundleActions* actions)
{
    log_info("LINK_AVAILABLE *%p", event->link_);
    
    Link* link = event->link_;
    Peer* peer = link->peer();
    ASSERT(link->isavailable());

    // Move all bundles from peer queue to link queue
    actions->move_contents(peer, link);
    
    // If something is queued on the link open it
    if (link->size() > 0) {
        actions->open_link(link);
    }
}

/**
 * Default event handler when a link is unavailable
 */
void
BundleRouter::handle_link_unavailable(LinkUnavailableEvent* event,
                                      BundleActions* actions)
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
BundleRouter::handle_contact_up(ContactUpEvent* event,
                                BundleActions* actions)
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
    actions->move_contents(link, contact);

    // How about moving messages from Peer Queue to Link Queue
    /// XXX/Sushant is this the right place to do ? or fwd_next_hop
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
BundleRouter::handle_contact_down(ContactDownEvent* event,
                                  BundleActions* actions)
{

    Contact* contact = event->contact_;
    Link* link = contact->link();
    log_info("CONTACT_DOWN *%p: re-routing %d queued bundles",
             contact, contact->bundle_list()->size());
    
    ASSERT(link->bundle_list()->size() == 0);
    actions->move_contents(contact, link);
    
    // Close the contact
    actions->close_link(link);

    // Check is link is suppoed to be available and if yes
    // try to open the link

    if ((link->size() > 0) && link->isavailable()) {
        actions->open_link(link);
    }
}

/**
 * Default event handler when reassembly is completed. For each
 * bundle on the list, check the pending count to see if the
 * fragment can be deleted.
 */
void
BundleRouter::handle_reassembly_completed(ReassemblyCompletedEvent* event,
                                          BundleActions* actions)
{
    log_info("REASSEMBLY_COMPLETED bundle id %d",
             event->bundle_.bundle()->bundleid_);
    
    Bundle* bundle;
    while ((bundle = event->fragments_.pop_front()) != NULL) {
        if (bundle->pendingtx() == 0) {
            delete_from_pending(bundle, actions);
        }

        bundle->del_ref("bundle_list", event->fragments_.name().c_str());
    }

    // add the newly reassembled bundle to the pending list and the
    // data store
    bundle = event->bundle_.bundle();
    pending_bundles_->push_back(bundle, NULL);
    actions->store_add(bundle);
}

/**
 * Default event handler when a new route is added by the command
 * or management interface.
 *
 * Adds an entry to the route table, then walks the pending list
 * to see if any bundles match the new route.
 */
void
BundleRouter::handle_route_add(RouteAddEvent* event,
                               BundleActions* actions)
{
    RouteEntry* entry = event->entry_;
    route_table_->add_entry(entry);
    new_next_hop(entry->pattern_, entry->next_hop_, actions);
}

/**
 * Add an action to forward a bundle to a next hop route, making
 * sure to do reassembly if the forwarding action specifies as
 * such.
 */
void
BundleRouter::fwd_to_nexthop(Bundle* bundle, RouteEntry* nexthop,
                             BundleActions* actions)
{
    // handle reassembly for certain routes
    if (nexthop->action_ == FORWARD_REASSEMBLE && bundle->is_fragment_) {
        // pass the bundle to the fragment manager for reassembly. if
        // this was the last fragment, then we'll get back the
        // reassembled bundle
        bundle = FragmentManager::instance()->process(bundle);
        
        if (!bundle)
            return;
    }
    bundle->add_pending();
  
    // If nexthop->nexthop_ is a Link
    // and if it is available open it
    BundleConsumer* bc = nexthop->next_hop_;

    // XXX/demmer this sucks -- replace with a type code for the
    // bundle consumer, not just the type string.
    if (strcasecmp(bc->type_str(), "Link") == 0) {
        Link* link = (Link *)bc;
        if ((link->type() == Link::ONDEMAND) &&
            link->isavailable() && (!link->isopen()))
        {
            log_info("Opening ONDEMAND link %s because messages are queued for it",
                     link->name());

            actions->open_link(link);
        }
    }

    // Add message to the next hop queue
    actions->enqueue_bundle(bundle, nexthop->next_hop_,
                            nexthop->action_, nexthop->mapping_grp_);
}


/**
 * Get all matching entries from the routing table, and add a
 * corresponding forwarding action to the action list.
 *
 * Note that if the include_local flag is set, then local routes
 * (i.e. registrations) are included in the list.
 *
 * Returns the number of matches found and assigned.
 */
int
BundleRouter::fwd_to_matching(Bundle* bundle, BundleActions* actions,
                              bool include_local)
{
    RouteEntry* entry;
    RouteEntrySet matches;
    RouteEntrySet::iterator iter;

    route_table_->get_matching(bundle->dest_, &matches);
    
    int count = 0;
    for (iter = matches.begin(); iter != matches.end(); ++iter) {
        entry = *iter;

        if ((include_local == false) && entry->next_hop_->is_local())
            continue;

        fwd_to_nexthop(bundle, entry, actions);
        
        ++count;
    }

    log_debug("fwd_to_matching %s: %d matches", bundle->dest_.c_str(), count);
    return count;
}

/**
 * Called whenever a new consumer (i.e. registration or contact)
 * arrives. This walks the list of all pending bundles, forwarding
 * all matches to the new contact.
 */
void
BundleRouter::new_next_hop(const BundleTuplePattern& dest,
                           BundleConsumer* next_hop,
                           BundleActions* actions)
{
    // XXX/demmer WRONG WRONG WRONG -- should really only check the
    // NEW contact, not all the contacts.
    
    log_debug("new_next_hop %s: checking pending bundle list...",
              next_hop->dest_str());
    BundleList::iterator iter;

    oasys::ScopeLock l(pending_bundles_->lock());
    
    for (iter = pending_bundles_->begin();
         iter != pending_bundles_->end();
         ++iter)
    {
        fwd_to_matching(*iter, actions, true);
    }
}

/**
 * Delete the given bundle from the pending list (assumes the
 * pending count is zero).
 */
void
BundleRouter::delete_from_pending(Bundle* bundle, BundleActions* actions)
{
    // XXX/demmer should this submit a BUNDLE_DEQUEUE action, not
    // directly manipulate the list? if so then we need a "consumer"
    // for the pending bundles list
    
    ASSERT(bundle->pendingtx() == 0);
    
    log_debug("bundle %d: no more pending transmissions -- "
              "removing from pending list", bundle->bundleid_);

    actions->store_del(bundle);

    BundleMapping* mapping = bundle->get_mapping(pending_bundles_);
    ASSERT(mapping);
    pending_bundles_->erase(mapping->position_, NULL);
}

} // namespace dtn

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

#include "BundleRouter.h"
#include "RouteTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/Contact.h"
#include "bundling/FragmentManager.h"
#include "reg/Registration.h"
#include <stdlib.h>

#include "FloodBundleRouter.h"
#include <stdlib.h>

namespace dtn {

/**
 * Constructor.
 */
FloodBundleRouter::FloodBundleRouter()
    : all_tuples_("bundles://*/*:*")
{
    log_info("FLOOD_ROUTER");

//    router_stats_= new RouterStatistics(this);
}

/**
 * Destructor
 */
FloodBundleRouter::~FloodBundleRouter()
{
    NOTIMPLEMENTED;
}

/**
 * Default event handler for new bundle arrivals.
 *
 * Queue the bundle on the pending delivery list, and then
 * searches through the route table to find any matching next
 * contacts, filling in the action list with forwarding decisions.
 */
void
FloodBundleRouter::handle_bundle_received(BundleReceivedEvent* event,
                                          BundleActions* actions)
{
    Bundle* bundle = event->bundleref_.bundle();
    log_info("FLOOD: bundle_rcv bundle id %d", bundle->bundleid_);
    
    /*
     * Check if the bundle isn't complete. If so, do reactive
     * fragmentation.
     *
     * XXX/demmer this should maybe be changed to do the reactive
     * fragmentation in the forwarder?
     */
    if (event->bytes_received_ != bundle->payload_.length()) {
        log_info("XXX: PARTIAL bundle:%d, making fragment of %d bytes",
                  bundle->bundleid_,event->bytes_received_);
        BundleDaemon::instance()->fragmentmgr()->
            convert_to_fragment(bundle, event->bytes_received_);
    }
        

    //statistics
    //bundle has been accepted
    //router_stats_->add_bundle_hop(bundle);

    

    Bundle* iter_bundle;
    BundleList::iterator iter;

    oasys::ScopeLock lock(pending_bundles_->lock());
    
    // bundle is always pending until expired or acked
    // note: ack is not implemented
    bundle->add_pending();
    
    //check if we already have the bundle with us ... 
    //then dont enqueue it
    // upon arrival of new contact, send all pending bundles over contact
    for (iter = pending_bundles_->begin(); 
         iter != pending_bundles_->end(); ++iter) {
        iter_bundle = *iter;
        log_info("\tpending_bundle:%d size:%d pending:%d",
                  iter_bundle->bundleid_,iter_bundle->payload_.length(),
                  iter_bundle->pendingtx());
        if(iter_bundle->bundleid_ == bundle->bundleid_) {
            //delete the bundle
            return;
        }
    }
    
    pending_bundles_->push_back(bundle, NULL);
    if (event->source_ != EVENTSRC_PEER)
        actions->store_add(bundle);
    
    //here we do not need to handle the new bundle immediately
    //just put it in the pending_bundles_ queue, and it
    //needs to be used only when a new contact comes up
    //**might do something different if the bundle is from
    //  the local node
    //BundleRouter::handle_bundle_received(event, actions);

    fwd_to_matching(bundle,actions,true);
}

void
FloodBundleRouter::handle_bundle_transmitted(BundleTransmittedEvent* event,
                                        BundleActions* actions)
{
    Bundle* bundle = event->bundleref_.bundle();
    
    bundle->add_pending();
    
    //only call the fragmentation routine if we send nonzero bytes
    if(event->bytes_sent_ > 0) {
        BundleRouter::handle_bundle_transmitted(event,actions);         
    } else {
        log_info("FLOOD: transmitted ZERO bytes:%d",bundle->bundleid_);
    }
    
    //now we want to ask the contact to send the other queued
    //bundles it has

    PANIC("XXX/demmer need to kick the contact to tell it to send bundleS");
    //Contact * contact = (Contact *)event->consumer_;
    //contact->enqueue_bundle(NULL, FORWARD_UNIQUE);
    
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
FloodBundleRouter::handle_registration_added(RegistrationAddedEvent* event,
                                        BundleActions* actions)
{
    Registration* registration = event->registration_;
    log_info("FLOOD: new registration for %s", registration->endpoint().c_str());

    RouteEntry* entry = new RouteEntry(registration->endpoint(),
                                       registration, FORWARD_REASSEMBLE);
    route_table_->add_entry(entry);
 
    //BundleRouter::handle_registration_added(event,actions);
}

/**
 * Default event handler when a new link is created.
 */
void
FloodBundleRouter::handle_link_created(LinkCreatedEvent* event,
                                       BundleActions* actions)
{
    
    Link* link = event->link_;
    ASSERT(link != NULL);
    log_info("FLOOD: LINK_CREATED *%p", event->link_);

    RouteEntry* entry = new RouteEntry(all_tuples_, link, FORWARD_COPY);
    route_table_->add_entry(entry);

    //first clear the list with the contact
//    contact->bundle_list()->clear();

    //copy the pending_bundles_ list into a new exchange list
    //exchange_list_ = pending_bundles_->copy();
    //
    new_next_hop(all_tuples_, link, actions);
}

/**
 * Default event handler when a contact is down
 */
void
FloodBundleRouter::handle_contact_down(ContactDownEvent* event,
                                    BundleActions* actions)
{
    Contact* contact = event->contact_;
    log_info("FLOOD: CONTACT_DOWN *%p: removing queued bundles", contact);
    
    //XXX not implemented yet - neeed to do
    route_table_->del_entry(all_tuples_, contact);
    // empty contact list
    // for flood, no need to maintain bundle list
    contact->bundle_list()->clear();

    
    //dont close the contact -- we have long running ones
    //this will PANIC
    //contact->close();
}

void
FloodBundleRouter::handle_route_add(RouteAddEvent* event,
                               BundleActions* actions)
{
    BundleRouter::handle_route_add(event, actions);     
}

/**
 * Called whenever a new consumer (i.e. registration or contact)
 * arrives. This should walk the list of unassigned bundles (or
 * maybe all bundles???) to see if the new consumer matches.
 */
void
FloodBundleRouter::new_next_hop(const BundleTuplePattern& dest,
                           BundleConsumer* next_hop,
                           BundleActions* actions)
{
    log_debug("FLOOD:  new_next_hop");

    Bundle* bundle;
    BundleList::iterator iter;

    oasys::ScopeLock lock(pending_bundles_->lock());
    
    // upon arrival of new contact, send all pending bundles over contact
    for (iter = pending_bundles_->begin(); 
         iter != pending_bundles_->end(); ++iter) {
        bundle = *iter;
        actions->enqueue_bundle(bundle, next_hop, FORWARD_COPY);
    }
}


int
FloodBundleRouter::fwd_to_matching(
                    Bundle* bundle, BundleActions* actions, bool include_local)
{
    RouteEntry* entry;
    RouteEntrySet matches;
    RouteEntrySet::iterator iter;

    route_table_->get_matching(bundle->dest_, &matches);
    
    int count = 0;
    for (iter = matches.begin(); iter != matches.end(); ++iter) {
        entry = *iter;
        log_info("\tentry: point:%s --> %s [%s] local:%d",
                entry->pattern_.c_str(),
                entry->next_hop_->dest_str(),
                bundle_fwd_action_toa(entry->action_),
                entry->next_hop_->is_local());
        if (!entry->next_hop_->is_local())
            continue;
        
        actions->enqueue_bundle(bundle, entry->next_hop_, entry->action_);
        ++count;
        bundle->add_pending();
    }

    log_info("FLOOD: local_fwd_to_matching %s: %d matches", bundle->dest_.c_str(), count);
    return count;
}

} // namespace dtn

/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
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
#ifndef _TABLE_BASED_ROUTER_H_
#define _TABLE_BASED_ROUTER_H_

#include "BundleRouter.h"

namespace dtn {

class RouteTable;

/**
 * This is the implementation of the basic bundle routing algorithm
 * that only does static routing. Routes can be parsed from a
 * configuration file or injected via the command line and/or
 * management interfaces.
 *
 * As a result, the class simply uses the default base class handlers
 * for all bundle events that flow through it, and the only code here
 * is that which parses the static route configuration file.
 */
 
class TableBasedRouter : public BundleRouter {
protected:
    /**
     * Constructor -- protected since this class is never instantiated
     * by itself.
     */
    TableBasedRouter(const std::string& name);

    /**
     * Event handler overridden from BundleRouter / BundleEventHandler
     * that dispatches to the type specific handlers where
     * appropriate.
     */
    void handle_event(BundleEvent* event);

    /**
     * Handler for new bundle arrivals simply forwards the newly arrived
     * bundle to all matching entries in the table.
     */
    void handle_bundle_received(BundleReceivedEvent* event);
    
    /**
     * Default event handler when a new route is added by the command
     * or management interface.
     *
     * Adds an entry to the route table, then walks the pending list
     * to see if any bundles match the new route.
     */
    void handle_route_add(RouteAddEvent* event);

    /**
     * When a contact comes up, check to see if there are any matching
     * bundles for it.
     */
    void handle_contact_up(ContactUpEvent* event);

    /**
     * Ditto if a link becomes available.
     */
    void handle_link_available(LinkAvailableEvent* event);

    /**
     * And again if we've sent a bundle on the link (since it may have
     * been busy.
     */
    void handle_bundle_transmitted(BundleTransmittedEvent* event);
        
    /**
     * Dump the routing state.
     */
    void get_routing_state(oasys::StringBuffer* buf);
    
    /**
     * Add a route entry to the routing table. 
     */
    void add_route(RouteEntry *entry);

    /**
     * Add an action to forward a bundle to a next hop route, making
     * sure to do reassembly if the forwarding action specifies as
     * such.
     */
    virtual void fwd_to_nexthop(Bundle* bundle, RouteEntry* nexthop);
     
    
    /**
     * Check the route table entries that match the given bundle and
     * have not already been found in the bundle history. If a match
     * is found, call fwd_to_nexthop on it.
     *
     * @param bundle	the bundle to forward
     * @param next_hop	if specified, restricts forwarding to the given
     * 			next hop link
     *
     * Returns the number of matches found and assigned.
     */
    virtual int fwd_to_matching(Bundle* bundle, Link* next_hop = NULL);

    /**
     * Called when the next hop link is available for transmission
     * (i.e. either when it first arrives and the contact is brought
     * up or when a bundle is completed and it's no longer busy).
     *
     * Loops through the bundle list and calls fwd_to_matching on all
     * bundles.
     */
    virtual void check_next_hop(Link* next_hop);

    /// The static routing table
    RouteTable* route_table_;
};

} // namespace dtn

#endif /* _TABLE_BASED_ROUTER_H_ */

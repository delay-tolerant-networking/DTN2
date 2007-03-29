/*
 *    Copyright 2005-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _TABLE_BASED_ROUTER_H_
#define _TABLE_BASED_ROUTER_H_

#include "BundleRouter.h"

namespace dtn {

class RouteTable;

/**
 * This is an abstract class that is intended to be used for all
 * routing algorithms that store routing state in a table.
 */
class TableBasedRouter : public BundleRouter {
protected:
    /**
     * Constructor -- protected since this class is never instantiated
     * by itself.
     */
    TableBasedRouter(const char* classname, const std::string& name);

    /**
     * Destructor.
     */
    virtual ~TableBasedRouter();

    /**
     * Event handler overridden from BundleRouter / BundleEventHandler
     * that dispatches to the type specific handlers where
     * appropriate.
     */
    virtual void handle_event(BundleEvent* event);

    /**
     * Handler for new bundle arrivals simply forwards the newly arrived
     * bundle to all matching entries in the table.
     */
    virtual void handle_bundle_received(BundleReceivedEvent* event);
    
    /**
     * Handler for transmission failures.
     */
    //virtual void handle_bundle_transmit_failed(BundleTransmitFailedEvent* event);
    
    /**
     * Default event handler when a new route is added by the command
     * or management interface.
     *
     * Adds an entry to the route table, then walks the pending list
     * to see if any bundles match the new route.
     */
    virtual void handle_route_add(RouteAddEvent* event);

    /**
     * Default event handler when a route is deleted by the command
     * or management interface.
     */
    virtual void handle_route_del(RouteDelEvent* event);
    
    /**
     * When a contact comes up, check to see if there are any matching
     * bundles for it.
     */
    virtual void handle_contact_up(ContactUpEvent* event);

    /**
     * Ditto if a link becomes available.
     */
    virtual void handle_link_available(LinkAvailableEvent* event);

    /**
     * If a link gets created with a remote eid, add the route
     */
    virtual void handle_link_created(LinkCreatedEvent* event);

    /**
     * Delete all routes that depend on the given link.
     */
    virtual void handle_link_deleted(LinkDeletedEvent* event);

    /**
     * Handle a custody transfer timeout.
     */
    virtual void handle_custody_timeout(CustodyTimeoutEvent* event);
    
    /**
     * Dump the routing state.
     */
    void get_routing_state(oasys::StringBuffer* buf);
    
    /**
     * Add a route entry to the routing table. 
     */
    void add_route(RouteEntry *entry);

    /**
     * Remove matrhing route entry(s) from the routing table. 
     */
    void del_route(const EndpointIDPattern& id);

    /**
     * Try to forward a bundle to a next hop route.
     */
    virtual void fwd_to_nexthop(Bundle* bundle, RouteEntry* route);

    /**
     * Check if the bundle should be forwarded to the given next hop.
     * Reasons why it would not be forwarded include that it was
     * already transmitted or is currently in flight on the link, or
     * that the route indicates ForwardingInfo::FORWARD_ACTION and it
     * is already in flight on another route.
     */
    virtual bool should_fwd(const Bundle* bundle, RouteEntry* route);
    
    /**
     * Check the route table entries that match the given bundle and
     * have not already been found in the bundle history. If a match
     * is found, call fwd_to_nexthop on it.
     *
     * @param bundle		the bundle to forward
     * @param this_link_only	if specified, restricts forwarding to
     * 				the given next hop link
     *
     * Returns the number of matches found and assigned.
     */
    virtual int fwd_to_matching(Bundle* bundle, const LinkRef& this_link_only);

    virtual int fwd_to_matching(Bundle* bundle) {
        LinkRef link("TableBasedRouter::fwd_to_matching: null");
        return fwd_to_matching(bundle, link);
    }
    
    /**
     * Called when the next hop link is available for transmission
     * (i.e. either when it first arrives and the contact is brought
     * up or when a bundle is completed and it's no longer busy).
     *
     * Loops through the bundle list and calls fwd_to_matching on all
     * bundles.
     */
    virtual void check_next_hop(const LinkRef& next_hop);

    /**
     * When new links are added or opened, and if we're configured to
     * add nexthop routes, try to add a new route for the given link.
     */
    void add_nexthop_route(const LinkRef& link);

    /// The static routing table
    RouteTable* route_table_;
};

} // namespace dtn

#endif /* _TABLE_BASED_ROUTER_H_ */

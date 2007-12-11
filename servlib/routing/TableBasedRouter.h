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

#include <oasys/util/StringUtils.h>

#include "BundleRouter.h"
#include "DuplicateCache.h"

namespace dtn {

class BundleList;
class RouteEntryVec;
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
    
    /// @{ Event handlers
    virtual void handle_bundle_received(BundleReceivedEvent* event);
    virtual void handle_bundle_transmitted(BundleTransmittedEvent* event);
    //virtual void handle_bundle_transmit_failed(BundleTransmitFailedEvent* event);
    virtual void handle_bundle_cancelled(BundleSendCancelledEvent* event);
    virtual void handle_route_add(RouteAddEvent* event);
    virtual void handle_route_del(RouteDelEvent* event);
    virtual void handle_contact_up(ContactUpEvent* event);
    virtual void handle_contact_down(ContactDownEvent* event);
    virtual void handle_link_available(LinkAvailableEvent* event);
    virtual void handle_link_created(LinkCreatedEvent* event);
    virtual void handle_link_deleted(LinkDeletedEvent* event);
    virtual void handle_custody_timeout(CustodyTimeoutEvent* event);
    /// @}
    
    /**
     * Dump the routing state.
     */
    void get_routing_state(oasys::StringBuffer* buf);

    /**
     * Get a tcl version of the routing state.
     */
    void tcl_dump_state(oasys::StringBuffer* buf);

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
    virtual bool fwd_to_nexthop(Bundle* bundle, RouteEntry* route);

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
     *
     * Returns the number of links on which the bundle was queued
     * (i.e. the number of matching route entries.
     */
    virtual int route_bundle(Bundle* bundle);

    /**
     * Once a vector of matching routes has been found, sort the
     * vector. The default uses the route priority, breaking ties by
     * using the number of bytes queued.
     */
    virtual void sort_routes(Bundle* bundle, RouteEntryVec* routes);
    
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
     * Go through all known bundles in the system and try to re-route them.
     */
    virtual void reroute_all_bundles();

    /**
     * When new links are added or opened, and if we're configured to
     * add nexthop routes, try to add a new route for the given link.
     */
    void add_nexthop_route(const LinkRef& link);

    /// Cache to check for duplicates
    DuplicateCache dupcache_;

    /// The routing table
    RouteTable* route_table_;

    /// Timer class used to cancel transmission on down links after
    /// waiting for them to potentially reopen
    class RerouteTimer : public oasys::Timer {
    public:
        RerouteTimer(TableBasedRouter* router, const LinkRef& link)
            : router_(router), link_(link) {}
        virtual ~RerouteTimer() {}
        
        void timeout(const struct timeval& now);

    protected:
        TableBasedRouter* router_;
        LinkRef link_;
    };

    friend class RerouteTimer;

    /// Helper function for rerouting
    void reroute_bundles(const LinkRef& link);
    
    /// Table of reroute timers, indexed by the link name
    typedef oasys::StringMap<RerouteTimer*> RerouteTimerMap;
    RerouteTimerMap reroute_timers_;
};

} // namespace dtn

#endif /* _TABLE_BASED_ROUTER_H_ */

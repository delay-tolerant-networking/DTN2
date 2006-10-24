/*
 *    Copyright 2006 Intel Corporation
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

#ifndef _BUNDLE_ROUTEENTRY_H_
#define _BUNDLE_ROUTEENTRY_H_

#include <oasys/debug/Formatter.h>

#include "bundling/CustodyTimer.h"
#include "bundling/ForwardingInfo.h"
#include "naming/EndpointID.h"

namespace dtn {

class Link;
class RouteEntryInfo;

/**
 * Class to represent route table entry.
 *
 * Each entry contains an endpoint id pattern that is matched against
 * the destination address in the various bundles to determine if the
 * route entry should be used for the bundle.
 *
 * An entry also has a forwarding action type code which indicates
 * whether the bundle should be forwarded to this next hop and others
 * (ForwardingInfo::COPY_ACTION) or sent only to the given next hop
 * (ForwardingInfo::FORWARD_ACTION). The entry also stores the custody
 * transfer timeout parameters, unique for a given route.
 *
 * There is also a pointer to either an interface or a link for each
 * entry. In case the entry contains a link, then that link will be
 * used to send the bundle. If there is no link, there must be an
 * interface. In that case, bundles which match the entry will cause
 * the router to create a new link to the given endpoint whenever a
 * bundle arrives that matches the route entry. This new link is then
 * typically added to the route table.
 */
class RouteEntry : public oasys::Formatter {
public:
    /**
     * Default constructor requires a destination pattern and a link.
     */
    RouteEntry(const EndpointIDPattern& dest_pattern, Link* link);

    /**
     * Destructor.
     */
    ~RouteEntry();

    /**
     * Hook to parse route configuration options.
     */
    int parse_options(int argc, const char** argv,
                      const char** invalidp = NULL);

    /**
     * Virtual from formatter.
     */
    int format(char* buf, size_t sz) const;
     
    /**
     * Dump a header string in preparation for subsequent calls to dump();
     */
    static void dump_header(oasys::StringBuffer* buf);
    
    /**
     * Dump a string representation of the route entry. Any endpoint
     * ids that don't fit into the column width get put into the
     * long_eids vector.
     */
    void dump(oasys::StringBuffer* buf, EndpointIDVector* long_eids) const;
    
    /// The pattern that matches bundles' destination eid
    EndpointIDPattern dest_pattern_;

    /// The pattern that matches bundles' source eid
    EndpointIDPattern source_pattern_;
    
    /// Bit vector of the bundle priority classes that should match this route
    u_int bundle_cos_;
    
    /// Route priority
    u_int route_priority_;
        
    /// Next hop link
    Link* next_hop_;
        
    /// Forwarding action code 
    ForwardingInfo::action_t action_;

    /// Custody timer specification
    CustodyTimerSpec custody_timeout_;

    /// Abstract pointer to any algorithm-specific state that needs to
    /// be stored in the route entry
    RouteEntryInfo* info_;        
    
    // XXX/demmer confidence? latency? capacity?
    // XXX/demmer bit to distinguish
    // XXX/demmer make this serializable?
};

/**
 * Class for a vector of route entries. Used for the route table
 * itself and for what is returned in get_matching().
 */
class RouteEntryVec : public std::vector<RouteEntry*> {
public:
    /**
     * Sort the entries in the vector in descending priority order,
     * i.e. the highest priority first. In case of a tie, select the
     * link with fewer bytes in flight.
     */
    void sort_by_priority();
};

/**
 * Interface for any per-entry routing algorithm state.
 */
class RouteEntryInfo {
public:
    virtual ~RouteEntryInfo() {}
};

} // namespace dtn

#endif /* _BUNDLE_ROUTEENTRY_H_ */

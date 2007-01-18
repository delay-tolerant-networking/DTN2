/*
 *    Copyright 2004-2006 Intel Corporation
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

#ifndef _BUNDLE_ROUTETABLE_H_
#define _BUNDLE_ROUTETABLE_H_

#include <set>
#include <oasys/debug/Log.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/serialize/Serialize.h>

#include "RouteEntry.h"

namespace dtn {

/**
 * Class that implements the routing table, implemented
 * with an stl vector.
 */
class RouteTable : public oasys::Logger {
public:
    /**
     * Constructor
     */
    RouteTable(const std::string& router_name);

    /**
     * Destructor
     */
    virtual ~RouteTable();

    /**
     * Add a route entry.
     */
    bool add_entry(RouteEntry* entry);
    
    /**
     * Remove a route entry.
     */
    bool del_entry(const EndpointIDPattern& dest, const LinkRef& next_hop);
    
    /**
     * Remove all entries to the given endpoint id pattern.
     *
     * @return the number of entries removed
     */
    size_t del_entries(const EndpointIDPattern& dest);
    
    /**
     * Remove all entries that rely on the given next_hop link
     *
     * @return the number of entries removed
     **/
    size_t del_entries_for_nexthop(const LinkRef& next_hop);

    /**
     * Fill in the entry_set with the list of all entries whose
     * patterns match the given eid and next hop. If the next hop is
     * NULL, it is ignored.
     *
     * @return the count of matching entries
     */
    size_t get_matching(const EndpointID& eid, const LinkRef& next_hop,
                        RouteEntryVec* entry_set) const;
    
    /**
     * Syntactic sugar to call get_matching for all links.
     *
     * @return the count of matching entries
     */
    size_t get_matching(const EndpointID& eid,
                        RouteEntryVec* entry_set) const
    {
        LinkRef link("RouteTable::get_matching: null");
        return get_matching(eid, link, entry_set);
    }

    /**
     * Dump a string representation of the routing table.
     */
    void dump(oasys::StringBuffer* buf, EndpointIDVector* long_eids) const;

    /**
     * Return the size of the table.
     */
    int size() { return route_table_.size(); }

    /**
     * Return the routing table.  Asserts that the RouteTable
     * spin lock is held by the caller.
     */
    const RouteEntryVec *route_table();

    /**
     * Accessor for the RouteTable internal lock.
     */
    oasys::Lock* lock() { return &lock_; }

protected:
    /// The routing table itself
    RouteEntryVec route_table_;

    /**
     * Lock to protect internal data structures.
     */
    mutable oasys::SpinLock lock_;
};

} // namespace dtn

#endif /* _BUNDLE_ROUTETABLE_H_ */

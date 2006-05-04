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
#ifndef _BUNDLE_ROUTETABLE_H_
#define _BUNDLE_ROUTETABLE_H_

#include <set>
#include <oasys/debug/Log.h>
#include <oasys/util/StringBuffer.h>

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
    bool del_entry(const EndpointIDPattern& dest, Link* next_hop);
    
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
    size_t del_entries_for_nexthop(Link* next_hop);

    /**
     * Fill in the entry_set with the list of all entries whose
     * patterns match the given eid and next hop. If the next hop is
     * NULL, it is ignored.
     *
     * @return the count of matching entries
     */
    size_t get_matching(const EndpointID& eid, Link* next_hop,
                        RouteEntryVec* entry_set) const;
    
    /**
     * Syntactic sugar to call get_matching for all links.
     *
     * @return the count of matching entries
     */
    size_t get_matching(const EndpointID& eid,
                        RouteEntryVec* entry_set) const
    {
        return get_matching(eid, NULL, entry_set);
    }
    
    /**
     * Dump a string representation of the routing table.
     */
    void dump(oasys::StringBuffer* buf) const;

    /**
     * Return the size of the table.
     */
    int size() { return route_table_.size(); }

protected:
    /// The routing table itself
    RouteEntryVec route_table_;
};

} // namespace dtn

#endif /* _BUNDLE_ROUTETABLE_H_ */

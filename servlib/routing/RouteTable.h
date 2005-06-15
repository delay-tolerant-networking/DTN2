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

#include "bundling/BundleActions.h"
#include "bundling/BundleTuple.h"

namespace dtn {

class BundleConsumer;
class RouteEntryInfo;

/**
 * A route table entry. Each entry contains a tuple pattern that is
 * matched against the destination address of bundles to determine if
 * the bundle should be forwarded to the next hop. XXX/demmer more
 */
class RouteEntry {
public:
    RouteEntry(const BundleTuplePattern& pattern,
               BundleConsumer* next_hop,
               bundle_fwd_action_t action);

    ~RouteEntry();

    /// The destination pattern that matches bundles
    BundleTuplePattern pattern_;

    // XXX/demmer this should be a bitmask of flags
    /// Forwarding action code 
    bundle_fwd_action_t action_;

    /// Mapping group
    int mapping_grp_;
        
    /// Next hop (registration or contact).
    BundleConsumer* next_hop_;

    /// Abstract pointer to any algorithm-specific state that needs to
    /// be stored in the route entry
    RouteEntryInfo* info_;        
    
    // XXX/demmer confidence? latency? capacity?
    // XXX/demmer bit to distinguish
    // XXX/demmer make this serializable?
};

/**
 * Typedef for a set of route entries. Used for the route table itself
 * and for what is returned in get_matching().
 */
typedef std::vector<RouteEntry*> RouteEntrySet;

/**
 * Class that implements the routing table, currently implemented
 * using a stl set.
 */
class RouteTable : public oasys::Logger {
public:
    /**
     * Constructor
     */
    RouteTable();

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
    bool del_entry(const BundleTuplePattern& dest,
                   BundleConsumer* next_hop);

    /**
     * Remove all entries that rely on the given next_hop
     **/
    bool del_entries_for_nexthop(BundleConsumer* next_hop);

    /**
     * Fill in the entry_set with the list of all entries whose
     * patterns match the given tuple.
     *
     * @return the count of matching entries
     */
    size_t get_matching(const BundleTuple& tuple,
                        RouteEntrySet* entry_set) const;
    
    /**
     * Dump a string representation of the routing table.
     */
    void dump(oasys::StringBuffer* buf) const;

    int size() { return route_table_.size(); }

protected:
    /// The routing table itself
    RouteEntrySet route_table_;
};

/**
 * Interface for any per-entry routing algorithm state.
 */
class RouteEntryInfo {
public:
    virtual ~RouteEntryInfo() {}
};

} // namespace dtn

#endif /* _BUNDLE_ROUTETABLE_H_ */

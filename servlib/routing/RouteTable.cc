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

#include "RouteTable.h"
#include "contacts/Link.h"

namespace dtn {

/**
 * Constructor
 */
RouteTable::RouteTable(const std::string& router_name)
    : Logger("RouteTable", "/dtn/routing/%s/table", router_name.c_str())
{
}

/**
 * Destructor
 */
RouteTable::~RouteTable()
{
}

/**
 * Add a route entry.
 */
bool
RouteTable::add_entry(RouteEntry* entry)
{
    log_debug("add_route %s -> %s (%s)",
              entry->pattern_.c_str(),
              entry->next_hop_->nexthop(),
              bundle_fwd_action_toa(entry->action_));
    
    route_table_.push_back(entry);
    
    return true;
}

/**
 * Remove a route entry.
 */
bool
RouteTable::del_entry(const EndpointIDPattern& dest, Link* next_hop)
{
    RouteEntryVec::iterator iter;
    RouteEntry* entry;

    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        entry = *iter;

        if (entry->pattern_.equals(dest) && entry->next_hop_ == next_hop) {
            log_debug("del_route %s -> %s",
                      dest.c_str(), next_hop->nexthop());

            route_table_.erase(iter);
            delete entry;
            return true;
        }
    }    

    log_debug("del_route %s -> %s: no match!",
              dest.c_str(), next_hop->nexthop());
    return false;
}

/**
 * Remove all entries to the given endpoint id pattern.
 */
size_t
RouteTable::del_entries(const EndpointIDPattern& dest)
{
    RouteEntryVec::iterator iter;
    RouteEntry* entry;

    // since deleting from the middle of a vector invalidates
    // iterators for that vector, we have to loop multiple times until
    // we don't find any more entries that match
    int num_found = 0;
    bool found;
    do {
        found = false;
        for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
            entry = *iter;
            
            if (dest.equals(entry->pattern_)) {
                log_debug("del_route %s -> %s",
                          entry->pattern_.c_str(), entry->next_hop_->nexthop());
                
                route_table_.erase(iter);
                delete entry;
                found = true;
                ++num_found;
                break;
            }
        }
    } while (found);

    if (num_found == 0) {
        log_debug("del_entries %s: no matches!", dest.c_str());
    } else {
        log_debug("del_entries %s: removed %d routes", dest.c_str(), num_found);
    }
    
    return num_found;
}

size_t
RouteTable::del_entries_for_nexthop(Link* next_hop)
{
    RouteEntryVec::iterator iter;
    RouteEntry* entry;

    // since deleting from the middle of a vector invalidates
    // iterators for that vector, we have to loop multiple times.
    
    // since deleting from the middle of a vector invalidates
    // iterators for that vector, we have to loop multiple times until
    // we don't find any more entries that match
    int num_found = 0;
    bool found;
    do {
        found = false;
        for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
            entry = *iter;

            if (entry->next_hop_ == next_hop) {
                log_debug("del_route %s -> %s",
                          entry->pattern_.c_str(), next_hop->nexthop());

                route_table_.erase(iter);
                delete entry;
                found = true;
                ++num_found;
                break;
            }
        }
    } while (found);

    if (num_found == 0) {
        log_debug("del_entries_for_nexthop %s: no matches!",
                  next_hop->name());
    } else {
        log_debug("del_entries_for_nexthop %s: removed %d routes",
                  next_hop->name(), num_found);
    }
    
    return num_found;
}

/**
 * Fill in the entry_set with the list of all entries whose
 * patterns match the given eid and next hop.
 *
 * @return the count of matching entries
 */
size_t
RouteTable::get_matching(const EndpointID& eid, Link* next_hop,
                         RouteEntryVec* entry_vec) const
{
    RouteEntryVec::const_iterator iter;
    RouteEntry* entry;
    size_t count = 0;

    log_debug("get_matching %s", eid.c_str());
    
    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        entry = *iter;

        log_debug("check entry %s -> %s (%s)",
                  entry->pattern_.c_str(),
                  entry->next_hop_->nexthop(),
                  bundle_fwd_action_toa(entry->action_));
        
        if ((next_hop == NULL || entry->next_hop_ == next_hop) &&
            entry->pattern_.match(eid))
        {
            ++count;
            
            log_debug("match entry %s -> %s (%s)",
                      entry->pattern_.c_str(),
                      entry->next_hop_->nexthop(),
                      bundle_fwd_action_toa(entry->action_));

            entry_vec->push_back(entry);
        }
    }

    log_debug("get_matching %s done, %zu match(es)", eid.c_str(), count);
    return count;
}

/**
 * Dump the routing table.
 */
void
RouteTable::dump(oasys::StringBuffer* buf) const
{
    RouteEntryVec::const_iterator iter;
    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        buf->append("\t");
        (*iter)->dump(buf);
    }
}
} // namespace dtn

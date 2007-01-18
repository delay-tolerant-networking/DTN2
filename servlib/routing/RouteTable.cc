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


#include "RouteTable.h"

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
    log_debug("add_route *%p", entry);
    
    route_table_.push_back(entry);
    
    return true;
}

/**
 * Remove a route entry.
 */
bool
RouteTable::del_entry(const EndpointIDPattern& dest, const LinkRef& next_hop)
{
    oasys::ScopeLock l(&lock_, "RouteTable");

    RouteEntryVec::iterator iter;
    RouteEntry* entry;

    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        entry = *iter;

        if (entry->dest_pattern_.equals(dest) && entry->next_hop_ == next_hop) {
            log_debug("del_entry *%p", entry);
            
            route_table_.erase(iter);
            delete entry;
            return true;
        }
    }    

    log_debug("del_entry %s -> %s: no match!",
              dest.c_str(), next_hop->nexthop());
    return false;
}

/**
 * Remove all entries to the given endpoint id pattern.
 */
size_t
RouteTable::del_entries(const EndpointIDPattern& dest)
{
    oasys::ScopeLock l(&lock_, "RouteTable");

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
            
            if (dest.equals(entry->dest_pattern_)) {
                log_debug("del_route *%p", entry);
                
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
RouteTable::del_entries_for_nexthop(const LinkRef& next_hop)
{
    oasys::ScopeLock l(&lock_, "RouteTable");

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
                log_debug("del_route *%p", entry);

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
RouteTable::get_matching(const EndpointID& eid, const LinkRef& next_hop,
                         RouteEntryVec* entry_vec) const
{
    oasys::ScopeLock l(&lock_, "RouteTable");

    RouteEntryVec::const_iterator iter;
    RouteEntry* entry;
    size_t count = 0;

    log_debug("get_matching %s", eid.c_str());
    
    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        entry = *iter;

        log_debug("check entry *%p", entry);
        
        if ((next_hop == NULL || entry->next_hop_ == next_hop) &&
            entry->dest_pattern_.match(eid))
        {
            ++count;
            
            log_debug("match entry *%p", entry);
            
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
RouteTable::dump(oasys::StringBuffer* buf, EndpointIDVector* long_eids) const
{
    RouteEntry::dump_header(buf);
    RouteEntryVec::const_iterator iter;
    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        (*iter)->dump(buf, long_eids);
    }
}

/**
 * Return the routing table.  Asserts that the RouteTable spin lock is held
 * by the caller.
 */
const RouteEntryVec *
RouteTable::route_table()
{
    ASSERTF(lock_.is_locked_by_me(),
            "RouteTable::route_table must be called while holding lock");
    return &route_table_;
}

} // namespace dtn


#include "RouteTable.h"
#include "bundling/BundleConsumer.h"
#include "util/StringBuffer.h"

/**
 * RouteEntry constructor.
 */
RouteEntry::RouteEntry(const BundleTuplePattern& pattern,
                       BundleConsumer* next_hop,
                       bundle_action_t action)
    : pattern_(pattern),
      action_(action),
      next_hop_(next_hop),
      info_(NULL)
{
}

/**
 * RouteEntry destructor
 */
RouteEntry::~RouteEntry()
{
    if (info_)
        delete info_;
}

/**
 * Constructor
 */
RouteTable::RouteTable()
    : Logger("/route/table")
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
    // XXC/demmer check for duplicates?
    
    
    log_debug("add_route %s -> %s (%s)",
              entry->pattern_.c_str(),
              entry->next_hop_->dest_tuple()->c_str(),
              bundle_action_toa(entry->action_));
    
    route_table_.insert(entry);
    
    return true;
}
    
/**
 * Remove a route entry.
 */
bool
RouteTable::del_entry(const BundleTuplePattern& dest,
                      BundleConsumer* next_hop)
{
    RouteEntrySet::iterator iter;
    RouteEntry* entry;

    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        entry = *iter;

        if (entry->pattern_.equals(dest) && entry->next_hop_ == next_hop) {
            log_debug("del_route %s -> %s",
                      dest.c_str(),
                      next_hop->dest_tuple()->c_str());

            route_table_.erase(iter);
            return true;
        }
    }    

    log_debug("del_route %s -> %s: no match!",
              dest.c_str(),
              next_hop->dest_tuple()->c_str());
    return false;
}

/**
 * Fill in the entry_set with the list of all entries whose
 * patterns match the given tuple.
 *
 * @return the count of matching entries
 */
size_t
RouteTable::get_matching(const BundleTuple& tuple,
                         RouteEntrySet* entry_set) const
{
    RouteEntrySet::iterator iter;
    RouteEntry* entry;
    size_t count = 0;

    log_debug("get_matching %s", tuple.c_str());
    
    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        entry = *iter;

        if (entry->pattern_.match(tuple)) {
            ++count;
            
            log_debug("match entry %s -> %s (%s)",
                      entry->pattern_.c_str(),
                      entry->next_hop_->dest_tuple()->c_str(),
                      bundle_action_toa(entry->action_));

            entry_set->insert(entry);
        }
    }

    log_debug("get_matching %s done, %d match(es)", tuple.c_str(), count);
    return count;
}

/**
 * Dump the routing table.
 */
void
RouteTable::dump(StringBuffer* buf) const
{
    buf->append("route table:\n");

    RouteEntrySet::iterator iter;
    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        RouteEntry* entry = *iter;
        buf->appendf("\t%s -> %s (%s)\n",
                     entry->pattern_.c_str(),
                     entry->next_hop_->dest_tuple()->c_str(),
                     bundle_action_toa(entry->action_));
    }
}

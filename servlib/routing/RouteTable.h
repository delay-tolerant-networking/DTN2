#ifndef _BUNDLE_ROUTETABLE_H_
#define _BUNDLE_ROUTETABLE_H_

#include "bundling/BundleAction.h"
#include "bundling/BundleTuple.h"
#include "debug/Log.h"

class RouteEntryInfo;
class StringBuffer;

/**
 * A route table entry. Each entry contains a tuple pattern that is
 * matched against the destination address of bundles to determine if
 * the bundle should be forwarded to the next hop. XXX/demmer more
 */
class RouteEntry {
public:
    RouteEntry(const BundleTuplePattern& pattern,
               BundleConsumer* next_hop,
               bundle_action_t action);

    ~RouteEntry();

    /// The destination pattern that matches bundles
    BundleTuplePattern pattern_;

    /// Forwarding action code 
    bundle_action_t action_;
        
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
typedef std::set<RouteEntry*> RouteEntrySet;

/**
 * Class that implements the routing table, currently implemented
 * using a stl set.
 */
class RouteTable : public Logger {
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
    void dump(StringBuffer* buf) const;

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

#endif /* _BUNDLE_ROUTETABLE_H_ */

#ifndef _BUNDLE_ROUTE_H_
#define _BUNDLE_ROUTE_H_

#include <string>
#include <map>
#include "bundling/BundleTuple.h"
#include "bundling/Contact.h"
#include "debug/Debug.h"
#include "debug/Log.h"
#include "storage/Serialize.h"

class Bundle;
class Interface;

/**
 * The global bundle routing table.
 */
class RouteTable : public SerializableObject, public Logger {
public:
    /**
     * Singleton instance accessor.
     */
    static RouteTable* instance() {
        ASSERT(instance_ != NULL);
        return instance_;
    }

    /**
     * Boot time initializer.
     */
    static void init();

    /**
     * Constructor.
     */
    RouteTable();

    /**
     * Destructor.
     */
    virtual ~RouteTable();

    /**
     * Set the local region string.
     */
    void set_region(const std::string& local_region)
    {
        local_region_.assign(local_region);
    }
    
    /**
     * Add an route entry.
     */
    bool add_route(const std::string& dst_region,
                   const BundleTuple& next_hop,
                   contact_type_t type);

    /**
     * Remove a route entry.
     */
    bool del_route(const std::string& dst_region,
                   const BundleTuple& next_hop,
                   contact_type_t type);
    
    /**
     * Try to find an active next-hop contact for the given bundle.
     * Returns NULL if there is no matching valid.
     *
     * XXX/demmer this should be a list of contacts
     */
    Contact* next_hop(const Bundle* bundle);
    
    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(SerializeAction* a);

    /**
     * Debugging aid.
     */
    void dump();

protected:
    static RouteTable* instance_;
    std::string local_region_;

    typedef std::map<std::string, Contact*> EntryMap;
    typedef std::pair<EntryMap::iterator, bool> EntryMapInsertRet;

    EntryMap local_region_table_; // indexed by admin
    EntryMap inter_region_table_; // indexed by region
};

#endif /* _BUNDLE_ROUTE_H_ */

#ifndef _BUNDLE_ROUTE_H_
#define _BUNDLE_ROUTE_H_

#include <string>
#include "bundling/BundleTuple.h"
#include "bundling/Contact.h"
#include "debug/Debug.h"
#include "storage/Serialize.h"

class Bundle;
class Interface;

/**
 * An entry in the routing table.
 */
struct RouteEntry {
    BundleTuple next_hop_;	///< The admin name of the next hop
                                ///< bundle daemon.

    contact_type_t type_;	///< The contact type

    Interface* interface_;	///< Interface to send from
};

/**
 * The global bundle routing table.
 */
class RouteTable : public SerializableObject {
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
     * Set the local region string.
     */
    void set_region(const std::string& local_region);

    /**
     * Add an route entry.
     */
    void add_route(const std::string& dst_region,
                   const std::string& next_hop_region,
                   const std::string& next_hop_addr,
                   contact_type_t type);

    /**
     * Remove a route entry.
     */
    void del_route(const std::string& dst_region,
                   const std::string& next_hop_region,
                   const std::string& next_hop_addr,
                   contact_type_t type);
    
    /**
     * Try to find an active next-hop contact for the given bundle.
     * Returns NULL if there is no matching valid.
     */
    Contact* next_hop(Bundle* bundle);
    
    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(SerializeAction* a);

protected:
    static RouteTable* instance_;
};

#endif /* _BUNDLE_ROUTE_H_ */


#include "RouteTable.h"
#include "bundling/Bundle.h"

RouteTable* RouteTable::instance_ = NULL;

RouteTable::~RouteTable()
{
}

void
RouteTable::init()
{
    instance_ = new RouteTable();
}

void
RouteTable::serialize(SerializeAction* a)
{
}

RouteTable::RouteTable()
    : Logger("/route")
{
}

void
RouteTable::dump()
{
    EntryMap* table;
    EntryMap::iterator iter;

    log_info("Route Table Dump");
    log_info("(local region %s)", local_region_.c_str());
    
    table = &local_region_table_;
    log_info("local routes (%d entries):", table->size());
    for (iter = table->begin(); iter != table->end(); ++iter) {
        log_info("     %s -> *%p", iter->first.c_str(), iter->second);
    }

    table = &inter_region_table_;
    log_info("inter region routes (%d entries):", table->size());
    for (iter = table->begin(); iter != table->end(); ++iter) {
        log_info("     %s -> *%p", iter->first.c_str(), iter->second);
    }
}

Contact*
RouteTable::next_hop(const Bundle* b)
{
    EntryMap* table;
    EntryMap::iterator iter;

    if (local_region_.compare(b->dest_.region()) == 0) {
        log_debug("local route lookup of bundle dest %s...", b->dest_.c_str());

        table = &local_region_table_;
        
        // loop through the local entries, looking for a match between
        // admin part and a substring of the bundle destination
        for (iter = table->begin(); iter != table->end(); ++iter) {
            const std::string& admin = iter->first;
            const std::string& dst_admin = b->dest_.admin().substr(0, admin.length());

            log_debug("checking route entry %s, dest %s", admin.c_str(), dst_admin.c_str());
            
            if (admin.compare(dst_admin) == 0) {
                break; // found it
            }
        }
        
    } else {
        log_debug("inter region route lookup of bundle dest %s...", b->dest_.c_str());
        table = &inter_region_table_;
        iter = table->find(b->dest_.region());
    }
    
    if (iter == table->end()) {
        log_debug("no match...");
        return NULL;
    };

    log_debug("found *%p", iter->second);
    return iter->second;
}

/**
 * Add an route entry.
 */
bool
RouteTable::add_route(const std::string& dst_region,
                      const BundleTuple& next_hop,
                      contact_type_t type)
{
    // XXX/demmer what if there's already a contact for the next hop?
    // should we make a new one or add an entry to the map? the latter
    // implies we need refcounting on contacts
    
    // create the contact
    Contact* contact = new Contact(type, next_hop);

    EntryMapInsertRet ret;
    
    if (dst_region.compare(local_region_) == 0) {
        log_info("adding local route to %s", next_hop.c_str());
        
        EntryMap::value_type val(next_hop.admin(), contact);
        
        ret = local_region_table_.insert(val);
        
        if (! ret.second) {
            log_err("error: duplicate local route to%s", next_hop.c_str());
            delete contact;
            return false;
        }
    } else {
        log_info("adding inter-region route: region %s -> %s",
                 dst_region.c_str(), next_hop.c_str());
        
        EntryMap::value_type val(dst_region, contact);
        
        ret = inter_region_table_.insert(val);
        
        if (! ret.second) {
            log_err("error: duplicate inter region route: region %s -> %s",
                    dst_region.c_str(), next_hop.c_str());
            delete contact;
            return false;
        }
    }
    
    return true;
}

/**
 * Remove a route entry.
 */
bool
RouteTable::del_route(const std::string& dst_region,
               const BundleTuple& next_hop,
               contact_type_t type)
{
    return true;
}


#include "SQLBundleStore.h"
#include "bundling/Bundle.h"

#include <iostream>


using namespace std;


/******************************************************************************
 *
 * SQLBundleStore
 *
 *****************************************************************************/

/**
 * Constructor.
 */

SQLBundleStore::SQLBundleStore(const char* table_name, SQLImplementation* db)
    : BundleStore()
{
    Bundle tmpobj(this);

    store_ = new SQLStore(table_name,db);
    store_->create_table(&tmpobj);
    store_->set_key_name("bundleid");

    next_bundle_id_ = 0;
}



/**
 * Create a new bundle instance, then make a generic call into the
 * persistent store to look up the bundle and fill in it's members if
 * found.
 */
Bundle*
SQLBundleStore::get(int bundle_id)
{
    Bundle* bundle = new Bundle();
    if (store_->get(bundle, bundle_id) != 0) {
        delete bundle;
        return NULL;
    }

    return bundle;
}

/**
 * Store the given bundle in the persistent store.
 */
int
SQLBundleStore::put(Bundle* bundle)
{
    return store_->put(bundle);
}

/**
 * Delete the given bundle from the persistent store.
 */
int
SQLBundleStore::del(int bundle_id)
{
    return store_->del(bundle_id);
}



int 
SQLBundleStore::delete_expired(const time_t now) 
{
    const char* field = "expiration";
    StringBuffer query ;
    query.appendf("DELETE FROM %s WHERE %s > %lu", store_->table_name(), field, now);
    
    int retval = store_->exec_query(query.c_str());
    return retval;
}
     


bool
SQLBundleStore::is_custodian(int bundle_id) 
{

    NOTIMPLEMENTED;
    return -1;
}

#ifndef _SQL_BUNDLE_STORE_H_
#define _SQL_BUNDLE_STORE_H_

#include <sys/time.h>
#include "BundleStore.h"
#include "SQLStore.h"

#include "SQLImplementation.h"

/**
 * Implementation of a BundleStore that uses an underlying SQL
 * database.
 */

class SQLBundleStore : public BundleStore {
public:
    /**
     * Constructor -- takes as a parameter an abstract pointer to the
     * underlying storage technology so as to implement the basic
     * methods.
     */
    SQLBundleStore(const char*, SQLImplementation* db);
    //       : BundleStore(store) {}

    
    
    /// @{
    /**
     * Basic storage methods. 
     */
    Bundle* get(int bundle_id) ;
    int     put(Bundle* bundle);
    int     del(int bundle_id) ;
    /// @}
    
    /**
     * Delete expired bundles
     *
     * (was sweepOldBundles)
     */
    // REMOVE FROM bundles where bundles.expiration > now
    int delete_expired(const time_t now);
    
    /**
     * Return true if we're the custodian of the given bundle.
     * TODO: is this really needed??
     *
     * (was db_bundle_retain)
     */
    bool is_custodian(int bundle_id);

private:
    SQLStore* store_;
};

#endif /* _SQL_BUNDLE_STORE_H_ */

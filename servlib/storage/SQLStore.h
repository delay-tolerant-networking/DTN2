#ifndef dtn_sql_store_h
#define dtn_sql_store_h

#include <db.h>
#include "BundleStore.h"
#include "PersistentStore.h"

class SQLManager;

/**
 * Implementation of a StorageManager with an underlying SQL
 * database.
 */
class SQLStore : public PersistentStore {
public:
    SQLStore();
    
    /// @{ Virtual overrides from PersistentStore
    int get(SerializableObject* obj, const int key);
    int put(SerializableObject* obj, const int key);
    int del(const int key);
    int num_elements();
    void keys(std::vector<int> l);
    void elements(std::vector<SerializableObject*> l);
    /// @}
};


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
    SQLBundleStore(SQLStore* store)
        : BundleStore(store) {}

    /**
     * Destructor.
     */
    virtual ~SQLBundleStore();

    /**
     * Get a new bundle id, updating the value in the store
     *
     * (was db_update_bundle_id, db_restore_bundle_id)
     */
    int next_id();
    
    /**
     * Delete expired bundles
     *
     * (was sweepOldBundles)
     */
    int delete_expired(const time_t now)
    {
        // REMOVE FROM bundles where bundles.expiration > now
        return 0;
    }

    /**
     * Return true if we're the custodian of the given bundle.
     * TODO: is this really needed??
     *
     * (was db_bundle_retain)
     */
    virtual bool is_custodian(int bundle_id) = 0;
    
private:
    int next_bundle_id_; 	/// running serial number for bundles
    PersistentStore* store_;	/// abstract persistent storage implementation
};

#endif /* dtn_sql_store_h */

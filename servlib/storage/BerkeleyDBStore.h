#ifndef _BERKELEY_DB_STORE_H_
#define _BERKELEY_DB_STORE_H_

#include <db.h>
#include <list>
#include "PersistentStore.h"
#include "BundleStore.h"

class BerkeleyDBManager;
class SerializableObject;

/**
 * Implementation of a StorageManager with an underlying BerkeleyDB
 * database.
 */
class BerkeleyDBStore : public PersistentStore {
public:
    BerkeleyDBStore();
    
    /// @{ Virtual overrides from PersistentStore
    int get(SerializableObject* obj, const int key);
    int put(SerializableObject* obj, const int key);
    int del(const int key);
    int num_elements();
    void keys(std::list<int> l);
    void elements(std::list<SerializableObject*> l);
    /// @}
};


/**
 * Implementation of a BundleStore that uses an underlying BerkeleyDB
 * database.
 */

class BerkeleyDBBundleStore : public BundleStore {
public:
    /**
     * Constructor -- takes as a parameter an abstract pointer to the
     * underlying storage technology so as to implement the basic
     * methods.
     */
//     BerkeleyDBBundleStore(BerkeleyDBStore* store)
//         : BundleStore(store) {}

    /**
     * Destructor.
     */
    virtual ~BerkeleyDBBundleStore();

    /**
     * Get a new bundle id, updating the value in the store
     *
     * (was db_update_bundle_id, db_restore_bundle_id)
     */
    int next_id()
    {
        // start a txn, read the bundle id, update the bundle id,
        // write the txn
        return 0;
    }
    
    /**
     * Delete expired bundles
     *
     * (was sweepOldBundles)
     */
    int delete_expired(const time_t now)
    {
        // loop through the keys in the table, calling this->del(id);
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

#endif /* _BERKELEY_DB_STORE_H_ */

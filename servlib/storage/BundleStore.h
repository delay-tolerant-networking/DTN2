#ifndef _BUNDLE_STORE_H_
#define _BUNDLE_STORE_H_

#include <vector>

class Bundle;
class PersistentStore;
class StorageImpl;

typedef std::vector<Bundle*> BundleList;

/**
 * Abstract base class for bundle storage.
 */
class BundleStore {
public:
    /**
     * Constructor -- takes as a parameter an abstract pointer to the
     * underlying storage technology so as to implement the basic
     * methods.
     */
    BundleStore(PersistentStore* store);

    /**
     * Destructor.
     */
    virtual ~BundleStore();

    /// @{
    /**
     * Basic storage methods. These just dispatch to use the generic
     * PersistentStore interface.
     */
    Bundle* get(int bundle_id);
    int     put(Bundle* bundle);
    int     del(int bundle_id);
    /// @}
    
    /**
     * Get a new bundle id, updating the value in the store
     *
     * (was db_update_bundle_id, db_restore_bundle_id)
     */
    virtual int next_id() = 0;
    
    /**
     * Delete expired bundles
     *
     * (was sweepOldBundles)
     */
    virtual int delete_expired(const time_t now) = 0;

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

#endif /* _BUNDLE_STORE_H_ */

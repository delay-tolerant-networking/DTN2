#ifndef _BUNDLE_STORE_H_
#define _BUNDLE_STORE_H_

#include <vector>
#include "debug/Debug.h"

class Bundle;
class BundleList;
//class StorageImpl;

/**
 * Abstract base class for bundle storage.
 */
class BundleStore {
public:
    /**
     * Singleton instance accessor.
     */
    static BundleStore* instance() {
        if (instance_ == NULL) {
            PANIC("BundleStore::init not called yet");
        }
        return instance_;
    }

    /**
     * Boot time initializer that takes as a parameter the actual
     * instance to use.
     */
    static void init(BundleStore* instance) {
        ASSERT(instance_ == NULL);
        instance_ = instance;
    }
    
    /**
     * Constructor.
     */
    BundleStore();

    /**
     * Destructor.
     */
    virtual ~BundleStore();

    /// @{
    /**
     * Basic storage methods. These just dispatch to use the generic
     * PersistentStore interface.
     */
    virtual Bundle* get(int bundle_id) = 0;
    virtual int     put(Bundle* bundle) = 0;
    virtual int     put(Bundle* bundle, u_int32_t id) { return put (bundle) ; }
    virtual int     del(int bundle_id) = 0;
    /// @}
    
    /**
     * Get a new bundle id, updating the value in the store
     *
     * (was db_update_bundle_id, db_restore_bundle_id)
     */
    virtual int next_id();
    
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

protected:
    static BundleStore* instance_; ///< singleton instance

    int next_bundle_id_; 	/// running serial number for bundles
//    PersistentStore* store_;	/// abstract persistent storage implementation
};

#endif /* _BUNDLE_STORE_H_ */

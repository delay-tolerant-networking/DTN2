#ifndef _BUNDLE_STORE_H_
#define _BUNDLE_STORE_H_

#include <vector>
#include <oasys/debug/Debug.h>

class Bundle;
class BundleList;
class PersistentStore;
//class StorageImpl;

/**
 * Abstract base class for bundle storage.
 */
class BundleStore : public Logger {
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
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance_ != NULL); }
    
    /**
     * Constructor.
     */
    BundleStore(PersistentStore * store);

    /**
     * Destructor.
     */
    ~BundleStore();

    /**
     * Load in the stored bundles
     */
    bool load();

    /// @{
    /**
     * Basic storage methods.
     */
    Bundle*  get(int bundle_id);
    bool     add(Bundle* bundle);
    bool     update(Bundle* bundle);
    bool     del(int bundle_id);
    /// @}
    
//     /**
//      * Delete expired bundles
//      *
//      * (was sweepOldBundles)
//      */
//     virtual int delete_expired(const time_t now) = 0;

//     /**
//      * Return true if we're the custodian of the given bundle.
//      * TODO: is this really needed??
//      *
//      * (was db_bundle_retain)
//      */
//     virtual bool is_custodian(int bundle_id) = 0;

protected:
    static BundleStore* instance_; ///< singleton instance

    PersistentStore * store_;
};

#endif /* _BUNDLE_STORE_H_ */

#ifndef _GLOBAL_STORE_H_
#define _GLOBAL_STORE_H_

#include <oasys/debug/Debug.h>
#include <oasys/serialize/Serialize.h>

#include "PersistentStore.h"

class Globals : public SerializableObject
{
public:
    u_int32_t next_bundleid_;	///< running serial number for bundles
    u_int32_t next_regid_;	///< running serial number for registrations

    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(SerializeAction* a);

    /**
     * Destructor.
     */
    virtual ~Globals();
};



/**
 * Class for those elements of the router that need to
 * be persistently stored but are singleton global values. Examples
 * include the running sequence number for bundles and registrations,
 * as well as any persistent configuration settings.
 *
 * Unlike the other *Store instances, this class is itself a
 * serializable object, since it contains all the fields that need to
 * be stored.
 */
class GlobalStore : public Logger {
public:
    /**
     * Singleton instance accessor.
     */
    static GlobalStore* instance() {
        if (instance_ == NULL) {
            PANIC("GlobalStore::init not called yet");
        }
        return instance_;
    }

    /**
     * Boot time initializer that takes as a parameter the actual
     * instance to use.
     */
    static void init(GlobalStore* instance) {
        if (instance_ != NULL) {
            PANIC("GlobalStore::init called multiple times");
        }
        instance_ = instance;
    }
    
    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance_ != NULL); }
    
    /**
     * Constructor.
     */
    GlobalStore(PersistentStore * store);

    /**
     * Destructor.
     */
    virtual ~GlobalStore();

    /**
     * Get a new bundle id, updating the value in the store
     *
     * (was db_update_bundle_id, db_restore_bundle_id)
     */
    u_int32_t next_bundleid();
    
    /**
     * Get a new unique registration id, updating the running value in
     * the persistent table.
     *
     * (was db_new_regID, db_update_registration_id,
     * db_retable_registration_id)
     */
    u_int32_t next_regid();

    /**
     * Load in the globals.
     */
    bool load();

    /**
     * Update the globals in the store.
     */
    bool update();
    
    static GlobalStore* instance_; ///< singleton instance
    
    PersistentStore * store_;   ///< persistent storage object
    Globals globals;
};

#endif /* _GLOBAL_STORE_H_ */

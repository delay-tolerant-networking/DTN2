
#include "GlobalStore.h"
#include "StorageConfig.h"


GlobalStore* GlobalStore::instance_;

GlobalStore::GlobalStore(PersistentStore * store)
    : Logger("/storage/globals")
{
    globals.next_bundleid_ = 0;
    globals.next_regid_ = 10; // reg ids 0-9 are reserved
    store_ = store;
}

GlobalStore::~GlobalStore()
{
}

void
Globals::serialize(SerializeAction* a)
{
    a->process("next_bundleid", &next_bundleid_);
    a->process("next_regid",    &next_regid_);
}

Globals::~Globals()
{
}

/**
 * Get a new bundle id, updating the value in the store
 *
 * (was db_update_bundle_id, db_restore_bundle_id)
 */
u_int32_t
GlobalStore::next_bundleid()
{
    log_debug("next_bundleid %d -> %d", globals.next_bundleid_, globals.next_bundleid_ + 1);
    u_int32_t ret = globals.next_bundleid_++;
    if (! update()) {
        log_err("error updating globals table");
    }

    return ret;
}
    
/**
 * Get a new unique registration id, updating the running value in
 * the persistent table.
 *
 * (was db_new_regID, db_update_registration_id, db_retable_registration_id)
 */
u_int32_t
GlobalStore::next_regid()
{
    log_debug("next_regid %d -> %d", globals.next_regid_, globals.next_regid_ + 1);
    u_int32_t ret = globals.next_regid_++;
    if (! update()) {
        log_err("error updating globals table");
    }

    return ret;
}


bool
GlobalStore::load()
{
    log_debug("loading global store");

    int cnt = store_->num_elements();
    
    log_debug("count is %d ",cnt);
    if (cnt == 1) 
    {
        store_->get(&globals, 1);
        
        log_debug("loaded next bundle id %d next reg id %d",
                  globals.next_bundleid_, globals.next_regid_);

    } else if (cnt == 0 && StorageConfig::instance()->init_) {
        log_info("globals table does not exist... initializing it");
        
        globals.next_bundleid_ = 0;
        globals.next_regid_ = 0;
        
        if (store_->add(&globals, 1) != 0) 
        {
            log_err("error initializing globals table");
            exit(-1);
        }

    } else {
        log_err("error loading globals table (%d)", cnt);

        exit(-1);
    }

    return true;
}

bool GlobalStore::update()
{
    log_debug("updating global store");
    
    if (store_->update(&globals, 1) != 0) {
        log_err("error updating global store");
        return false;
    }
    
    return true;
}


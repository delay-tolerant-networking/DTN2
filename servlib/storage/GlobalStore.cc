
#include "GlobalStore.h"


GlobalStore* GlobalStore::instance_;

GlobalStore::GlobalStore()
    : Logger("/storage/globals")
{
}

GlobalStore::~GlobalStore()
{
}

void
GlobalStore::serialize(SerializeAction* a)
{
    a->process("next_bundleid", &next_bundleid_);
    a->process("next_regid",    &next_regid_);
}

/**
 * Get a new bundle id, updating the value in the store
 *
 * (was db_update_bundle_id, db_restore_bundle_id)
 */
u_int32_t
GlobalStore::next_bundleid()
{
    log_debug("next_bundleid %d -> %d", next_bundleid_, next_bundleid_ + 1);
    u_int32_t ret = next_bundleid_++;
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
    log_debug("next_regid %d -> %d", next_regid_, next_regid_ + 1);
    u_int32_t ret = next_regid_++;
    if (! update()) {
        log_err("error updating globals table");
    }

    return ret;
}



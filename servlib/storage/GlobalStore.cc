/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/storage/DurableStore.h>
#include <oasys/storage/StorageConfig.h>
#include <oasys/serialize/TypeShims.h>
#include <oasys/thread/Mutex.h>
#include <oasys/util/MD5.h>

#include "GlobalStore.h"
#include "bundling/Bundle.h"
#include "reg/APIRegistration.h"
#include "routing/ProphetNode.h"

namespace dtn {

//----------------------------------------------------------------------
const u_int32_t GlobalStore::CURRENT_VERSION = 3;
static const char* GLOBAL_TABLE = "globals";
static const char* GLOBAL_KEY   = "global_key";

//----------------------------------------------------------------------
class Globals : public oasys::SerializableObject
{
public:
    Globals() {}
    Globals(const oasys::Builder&) {}

    u_int32_t version_;         ///< on-disk copy of CURRENT_VERSION
    u_int32_t next_bundleid_;	///< running serial number for bundles
    u_int32_t next_regid_;	///< running serial number for registrations
    u_char digest_[oasys::MD5::MD5LEN];	///< MD5 digest of all serialized fields
    
    /**
     * Virtual from SerializableObject.
     */
    virtual void serialize(oasys::SerializeAction* a);
};

//----------------------------------------------------------------------
void
Globals::serialize(oasys::SerializeAction* a)
{
    a->process("version",       &version_);
    a->process("next_bundleid", &next_bundleid_);
    a->process("next_regid",    &next_regid_);
    a->process("digest",	digest_, 16);
}

//----------------------------------------------------------------------
GlobalStore* GlobalStore::instance_;

//----------------------------------------------------------------------
GlobalStore::GlobalStore()
    : Logger("GlobalStore", "/dtn/storage/%s", GLOBAL_TABLE),
      globals_(NULL), store_(NULL)
{
    lock_ = new oasys::Mutex(logpath_,
                             oasys::Mutex::TYPE_RECURSIVE,
                             true /* quiet */);
}

//----------------------------------------------------------------------
int
GlobalStore::init(const oasys::StorageConfig& cfg, 
                  oasys::DurableStore*        store)
{
    if (instance_ != NULL) 
    {
        PANIC("GlobalStore::init called multiple times");
    }
    
    instance_ = new GlobalStore();
    return instance_->do_init(cfg, store);
}

//----------------------------------------------------------------------
int
GlobalStore::do_init(const oasys::StorageConfig& cfg, 
                     oasys::DurableStore*        store)
{
    int flags = 0;

    // Global table has a single row using the (serialized) string key GLOBAL_KEY.
    // Just tell the database its a variable length string since it doesn't have to do
    // serious indexing.
    flags = (0 & KEY_LEN_MASK) << KEY_LEN_SHIFT;

    if (cfg.init_) {
        flags |= oasys::DS_CREATE;
    }

    int err = store->get_table(&store_, GLOBAL_TABLE, flags);

    if (err != 0) {
        log_err("error initializing global store: %s",
                (err == oasys::DS_NOTFOUND) ?
                "table not found" :
                "unknown error");
        return err;
    }

    // if we're initializing the database for the first time, then we
    // prime the values accordingly and sync the database version
    if (cfg.init_) 
    {
        log_info("initializing global table");

        globals_ = new Globals();

        globals_->version_       = CURRENT_VERSION;
        globals_->next_bundleid_ = 0;
        globals_->next_regid_    = Registration::MAX_RESERVED_REGID + 1;
        calc_digest(globals_->digest_);

        // store the new value
        err = store_->put(oasys::StringShim(GLOBAL_KEY), globals_,
                          oasys::DS_CREATE | oasys::DS_EXCL);
        
        if (err == oasys::DS_EXISTS) 
        {
            // YUCK
            log_err_p("/dtnd", "Initializing datastore which already exists.");
            exit(1);
        } else if (err != 0) {
            log_err_p("/dtnd", "unknown error initializing global store");
            return err;
        }
        
        loaded_ = true;
        
    } else {
        loaded_ = false;
    }

    return 0;
}

//----------------------------------------------------------------------
GlobalStore::~GlobalStore()
{
    delete store_;
    delete globals_;
    delete lock_;
}

//----------------------------------------------------------------------
u_int32_t
GlobalStore::next_bundleid()
{
    oasys::ScopeLock l(lock_, "GlobalStore::next_bundleid");
    
    ASSERT(globals_->next_bundleid_ != 0xffffffff);
    log_debug("next_bundleid %d -> %d",
              globals_->next_bundleid_,
              globals_->next_bundleid_ + 1);
    
    u_int32_t ret = globals_->next_bundleid_++;

    update();

    return ret;
}
    
//----------------------------------------------------------------------
u_int32_t
GlobalStore::next_regid()
{
    oasys::ScopeLock l(lock_, "GlobalStore::next_regid");
    
    ASSERT(globals_->next_regid_ != 0xffffffff);
    log_debug("next_regid %d -> %d",
              globals_->next_regid_,
              globals_->next_regid_ + 1);

    u_int32_t ret = globals_->next_regid_++;

    update();

    return ret;
}

//----------------------------------------------------------------------
void
GlobalStore::calc_digest(u_char* digest)
{
    // We create dummy objects for all serialized objects, then take
    // their serialized form and MD5 it, so adding or deleting a
    // serialized field will change the digest
    Bundle b(oasys::Builder::builder());
    APIRegistration r(oasys::Builder::builder());
    ProphetNode n(oasys::Builder::builder());

    oasys::StringSerialize s(oasys::Serialize::CONTEXT_LOCAL,
                             oasys::StringSerialize::INCLUDE_NAME |
                             oasys::StringSerialize::INCLUDE_TYPE |
                             oasys::StringSerialize::SCHEMA_ONLY);

    s.action(&b);
    s.action(&r);
    s.action(&n);

    oasys::MD5 md5;
    md5.update(s.buf().data(), s.buf().length());
    md5.finalize();

    log_debug("calculated digest %s for serialize string '%s'",
              md5.digest_ascii().c_str(), s.buf().c_str());

    memcpy(digest, md5.digest(), oasys::MD5::MD5LEN);
}

//----------------------------------------------------------------------
bool
GlobalStore::load()
{
    log_debug("loading global store");

    oasys::StringShim key(GLOBAL_KEY);

    if (globals_ != NULL) {
        delete globals_;
        globals_ = NULL;
    }

    if (store_->get(key, &globals_) != 0) {
        log_crit("error loading global data");
        return false;
    }
    ASSERT(globals_ != NULL);

    if (globals_->version_ != CURRENT_VERSION) {
        log_crit("datastore version mismatch: "
                 "expected version %d, database version %d",
                 CURRENT_VERSION, globals_->version_);
        return false;
    }

    u_char digest[oasys::MD5::MD5LEN];
    calc_digest(digest);

    if (memcmp(digest, globals_->digest_, oasys::MD5::MD5LEN) != 0) {
        log_crit("datastore digest mismatch: "
                 "expected %s, database contains %s",
                 oasys::hex2str(digest, oasys::MD5::MD5LEN).c_str(),
                 oasys::hex2str(globals_->digest_, oasys::MD5::MD5LEN).c_str());
        log_crit("(implies serialized schema change)");
        return false;
    }

    loaded_ = true;
    return true;
}

//----------------------------------------------------------------------
void
GlobalStore::update()
{
    ASSERT(lock_->is_locked_by_me());
    
    log_debug("updating global store");

    // make certain we don't attempt to write out globals before
    // load() has had a chance to load them from the database
    ASSERT(loaded_);

    oasys::DurableStore *store = oasys::DurableStore::instance();
    store->begin_transaction();

    int err = store_->put(oasys::StringShim(GLOBAL_KEY), globals_, 0);

    if (err != 0) {
        PANIC("GlobalStore::update fatal error updating database: %s",
              oasys::durable_strerror(err));
    }
}

//----------------------------------------------------------------------
void
GlobalStore::close()
{
    // we prevent the potential for shutdown race crashes by leaving
    // the global store locked after it's been closed so other threads
    // will simply block, not crash due to a null store
    lock_->lock("GlobalStore::close");
    
    delete store_;
    store_ = NULL;

    delete instance_;
    instance_ = NULL;
}

} // namespace dtn

/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _GLOBAL_STORE_H_
#define _GLOBAL_STORE_H_

#include <oasys/debug/DebugUtils.h>
#include <oasys/serialize/Serialize.h>

// forward decl
namespace oasys {
template<typename _Type> class SingleTypeDurableTable;
class DurableStore;
class StorageConfig;
}

namespace dtn {

class Globals;

/**
 * Class for those elements of the router that need to be persistently
 * stored but are singleton global values. Examples include the
 * running sequence number for bundles and registrations, as well as
 * any other persistent configuration settings.
 */
class GlobalStore : public oasys::Logger {
public:
    static const u_int32_t CURRENT_VERSION;
    
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
     * Boot time initializer
     *
     */
    static int init(const oasys::StorageConfig& cfg, 
                    oasys::DurableStore*        store);

    /**
     * Constructor.
     */
    GlobalStore();

    /**
     * Real initialization method.
     */
    int do_init(const oasys::StorageConfig& cfg, 
                oasys::DurableStore*        store);
    
    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance_ != NULL); }
    
    /**
     * Destructor.
     */
    ~GlobalStore();

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

    /**
     * Close (and flush) the data store.
     */
    void close();

protected:
    bool loaded_;
    Globals* globals_;
    oasys::SingleTypeDurableTable<Globals>* store_;

    static GlobalStore* instance_; ///< singleton instance
};
} // namespace dtn

#endif /* _GLOBAL_STORE_H_ */

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

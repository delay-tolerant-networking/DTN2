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

#include "GlobalStore.h"
#include "StorageConfig.h"
#include "reg/Registration.h"


GlobalStore* GlobalStore::instance_;

GlobalStore::GlobalStore(PersistentStore * store)
    : Logger("/storage/globals")
{
    globals.next_bundleid_ = 0;
    globals.next_regid_ = Registration::MAX_RESERVED_REGID + 1;
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

bool
GlobalStore::update()
{
    log_debug("updating global store");
    
    if (store_->update(&globals, 1) != 0) {
        log_err("error updating global store");
        return false;
    }
    
    return true;
}


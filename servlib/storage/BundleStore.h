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

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

#ifndef _BUNDLE_STORE_H_
#define _BUNDLE_STORE_H_

#include <oasys/debug/DebugUtils.h>
#include <oasys/serialize/TypeShims.h>
#include <oasys/storage/InternalKeyDurableTable.h>

namespace dtn {

class Bundle;

/**
 * Convenience typedef for the oasys template parent class.
 */
typedef oasys::InternalKeyDurableTable<
    oasys::UIntShim, u_int32_t, Bundle> BundleStoreImpl;

/**
 * The class for bundle storage is simply an instantiation of the
 * generic oasys durable table interface.
 */
class BundleStore : public BundleStoreImpl {
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
     * Boot time initializer that takes as a parameter the storage
     * configuration to use.
     */
    static int init(const oasys::StorageConfig& cfg,
                    oasys::DurableStore*        store) 
    {
        if (instance_ != NULL) {
            PANIC("BundleStore::init called multiple times");
        }
        instance_ = new BundleStore();
        return instance_->do_init(cfg, store);
    }
    
    /**
     * Constructor.
     */
    BundleStore();

    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance_ != NULL); }
    
protected:
    static BundleStore* instance_; ///< singleton instance
};

} // namespace dtn

#endif /* _BUNDLE_STORE_H_ */

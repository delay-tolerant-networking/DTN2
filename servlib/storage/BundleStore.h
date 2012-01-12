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
#include <oasys/storage/DurableStore.h>
#include <oasys/storage/InternalKeyDurableTable.h>
#include <oasys/util/OpenFdCache.h>
#include <oasys/util/Singleton.h>
#include "DTNStorageConfig.h"
#include "bundling/BundleDetail.h"


namespace dtn {

class Bundle;

/**
 * The class for bundle storage is an instantiation of an oasys
 * durable table to store the bundle metadata, tracking logic for the
 * storage total, and other support classes for the payloads.
 */
class BundleStore : public oasys::Singleton<BundleStore, false> {
public:
    /// Helper class typedefs
    typedef oasys::OpenFdCache<std::string> FdCache;
    typedef oasys::InternalKeyDurableTable<
        oasys::UIntShim, u_int32_t, Bundle> BundleTable;
    typedef BundleTable::iterator iterator;
#ifdef LIBODBC_ENABLED
    typedef oasys::InternalKeyDurableTable<
        oasys::UIntShim, u_int32_t, BundleDetail> BundleDetailTable;
#endif
    
    /**
     * Boot time initializer that takes as a parameter the storage
     * configuration to use.
     */
    static int init(const DTNStorageConfig& cfg,
                    oasys::DurableStore*    store);
    
    /**
     * Constructor.
     */
    BundleStore(const DTNStorageConfig& cfg);

    /// Add a new bundle
    bool add(Bundle* bundle);

    /// Retrieve a bundle
    Bundle* get(u_int32_t bundleid);
    
    /// Update the metabundle for the bundle
    bool update(Bundle* bundle);

    /// Delete the bundle
    bool del(Bundle* bundle);

    /// Return a new iterator
    iterator* new_iterator();
        
    /// Close down the table
    void close();

    /// @{ Accessors
    const std::string& payload_dir()     { return cfg_.payload_dir_; }
    u_int64_t          payload_quota()   { return cfg_.payload_quota_; }
    FdCache*           payload_fdcache() { return &payload_fdcache_; }
    u_int64_t          total_size()      { return total_size_; }
    /// @}
    
protected:
    friend class BundleDaemon;

    /// When the bundle store is loaded at boot time, we need to reset
    /// the in-memory total_size_ parameter
    void set_total_size(u_int64_t sz) { total_size_ = sz; }
    
    const DTNStorageConfig& cfg_; ///< Storage configuration
    BundleTable bundles_;	///< Bundle metabundle table
    FdCache payload_fdcache_;	///< File descriptor cache
    static bool using_aux_table_;	///< True when an auxiliary info table is configured and in use.
#ifdef LIBODBC_ENABLED
    BundleDetailTable bundle_details_;   ///< Auxiliary table for bundle unserialized details
#endif
    u_int64_t total_size_;	///M Total size in the data store
};

} // namespace dtn

#endif /* _BUNDLE_STORE_H_ */

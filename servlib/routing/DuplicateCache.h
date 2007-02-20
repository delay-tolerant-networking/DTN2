/*
 *    Copyright 2007 Intel Corporation
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

#ifndef _DUPLICATECACHE_H_
#define _DUPLICATECACHE_H_

#include <oasys/debug/Logger.h>
#include <oasys/util/Cache.h>
#include <oasys/util/CacheCapacityHelper.h>
#include "bundling/Bundle.h"

namespace dtn {

/**
 * Utility class for routers to use to maintain a cache of recently
 * received bundles, indexed by (source_eid, creation_timestamp).
 * Useful for routers to detect duplicate receptions.
 */
class DuplicateCache {
public:
    /**
     * Constructor that takes the number of entries to maintain.
     */
    DuplicateCache(size_t capacity);
    
    /**
     * Adds this bundle to the cache, returning whether or not it was
     * already in there (i.e. has been seen before);
     */
    bool is_duplicate(Bundle* bundle);

protected:
    /**
     * Cache key class.
     */
    struct Key {
        Key(EndpointID e, BundleTimestamp ts)
            : source_eid_(e), creation_ts_(ts) {}

        bool operator<(const Key& other) const
        {
            if (source_eid_ < other.source_eid_) {
                return true;
            } else if (other.source_eid_ < source_eid_) {
                return false;
            }
            
            return creation_ts_ < other.creation_ts_;
        }
        
        EndpointID      source_eid_;
        BundleTimestamp creation_ts_;
    };

    friend class oasys::InlineFormatter<Key>;

    /**
     * Cache value class.
     */
    struct Val {
        int count_;
    };

    /// The cache
    typedef oasys::CacheCapacityHelper<Key, Val> CacheCapacityHelper;
    typedef oasys::Cache<Key, Val, CacheCapacityHelper> Cache;
    Cache cache_;
};

} // namespace dtn


#endif /* _DUPLICATECACHE_H_ */

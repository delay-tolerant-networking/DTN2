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
#include "bundling/GbofId.h"

namespace dtn {

/**
 * Utility class for routers to use to maintain a cache of recently
 * received bundles, indexed by GbofId. Useful for routers to detect
 * duplicate receptions.
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

    /**
     * Flush the cache.
     */
    void evict_all();

protected:
    /**
     * Cache value class.
     */
    struct Val {
        int count_;
    };

    /// The cache
    typedef oasys::CacheCapacityHelper<GbofId, Val> CacheCapacityHelper;
    typedef oasys::Cache<GbofId, Val, CacheCapacityHelper> Cache;
    Cache cache_;
};

} // namespace dtn


#endif /* _DUPLICATECACHE_H_ */

/*
 *    Copyright 2007-2008 Intel Corporation
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

#ifndef _RECEPTIONCACHE_H_
#define _RECEPTIONCACHE_H_

#include <oasys/debug/Logger.h>
#include <oasys/util/Cache.h>
#include <oasys/util/CacheCapacityHelper.h>
#include "bundling/Bundle.h"
#include "bundling/GbofId.h"

namespace dtn {

/**
 * Utility class for routers to use to maintain a cache of recently
 * received bundles, indexed by GbofId. Useful for routers to detect
 * reception receptions.
 */
class ReceptionCache {
public:
    /**
     * Constructor that takes the number of entries to maintain.
     */
    ReceptionCache(size_t capacity);
    
    /**
     * Try to add the bundle to the cache. If it already exists in the
     * cache, adding it agail fails, and the method returns false.
     */
    bool add_entry(const Bundle* bundle, const EndpointID& prevhop);

    /**
     * Check if the given bundle is in the cache, returning the EID of
     * the node from which it arrived (if known).
     */
    bool lookup(const Bundle* bundle, EndpointID* prevhop);

    /**
     * Flush the cache.
     */
    void evict_all();

protected:
    typedef oasys::CacheCapacityHelper<GbofId, EndpointID> CacheCapacityHelper;
    typedef oasys::Cache<GbofId, EndpointID, CacheCapacityHelper> Cache;
    Cache cache_;
};

} // namespace dtn


#endif /* _RECEPTIONCACHE_H_ */

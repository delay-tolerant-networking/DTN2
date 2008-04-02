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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "DuplicateCache.h"

namespace dtn {

//----------------------------------------------------------------------
DuplicateCache::DuplicateCache(size_t capacity)
    : cache_("/dtn/route/duplicate_cache", CacheCapacityHelper(capacity))
{
}

//----------------------------------------------------------------------
bool
DuplicateCache::is_duplicate(Bundle* bundle)
{
    GbofId k(bundle->source(),
             bundle->creation_ts(),
             bundle->is_fragment(),
             bundle->payload().length(),
             bundle->frag_offset());
    Val v;
    if (cache_.get(k, &v)) {
        return true;
    }

    Cache::Handle h;
    v.count_ = 0;
    cache_.put_and_pin(k, v, &h);
    cache_.unpin(h);
    return false;
}

//----------------------------------------------------------------------
void
DuplicateCache::evict_all()
{
    cache_.evict_all();
}

} // namespace dtn

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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "ReceptionCache.h"

namespace dtn {

//----------------------------------------------------------------------
ReceptionCache::ReceptionCache(size_t capacity)
    : cache_("/dtn/route/reception_cache",
             CacheCapacityHelper(capacity),
             false /* no reorder on get() */ )
{
}

//----------------------------------------------------------------------
bool
ReceptionCache::add_entry(const Bundle* bundle, const EndpointID& prevhop)
{
    GbofId id(bundle->source(),
              bundle->creation_ts(),
              bundle->is_fragment(),
              bundle->payload().length(),
              bundle->frag_offset());
    
    Cache::Handle h;
    bool ok = cache_.put_and_pin(id, prevhop, &h);
    if (!ok) {
        return false;
    }
    
    h.unpin();
    return true;
}

//----------------------------------------------------------------------
bool
ReceptionCache::lookup(const Bundle* bundle, EndpointID* prevhop)
{
    GbofId id(bundle->source(),
              bundle->creation_ts(),
              bundle->is_fragment(),
              bundle->payload().length(),
              bundle->frag_offset());

    return cache_.get(id, prevhop);
}

//----------------------------------------------------------------------
void
ReceptionCache::evict_all()
{
    cache_.evict_all();
}

} // namespace dtn

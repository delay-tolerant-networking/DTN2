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

#include "DuplicateCache.h"

namespace oasys {

//----------------------------------------------------------------------
template<>
const char*
InlineFormatter<dtn::DuplicateCache::Key>
::format(const dtn::DuplicateCache::Key& k)
{
    buf_.appendf("<%s, %u.%u>",
                 k.source_eid_.c_str(),
                 k.creation_ts_.seconds_,
                 k.creation_ts_.seqno_);
    return buf_.c_str();
}
} // namespace oasys

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
    Key k(bundle->source_, bundle->creation_ts_);
    Val v;
    if (cache_.get(k, &v)) {
        return true;
    }

    Cache::Handle h;
    cache_.put_and_pin(k, v, &h);
    cache_.unpin(h);
    return false;
}

} // namespace dtn

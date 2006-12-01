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


#include "BundleStore.h"
#include "bundling/Bundle.h"

namespace dtn {

template <>
BundleStore* oasys::Singleton<BundleStore, false>::instance_ = 0;

//----------------------------------------------------------------------
BundleStore::BundleStore(const DTNStorageConfig& cfg)
    : cfg_(cfg),
      bundles_("BundleStore", "/dtn/storage/bundles",
               "bundle", "bundles"),
      payload_fdcache_("/dtn/storage/bundles/fdcache",
                       cfg.payload_fd_cache_size_),
      total_size_(0)
{
}
\
//----------------------------------------------------------------------
int
BundleStore::init(const DTNStorageConfig& cfg,
                  oasys::DurableStore*    store)
{
    if (instance_ != NULL) {
        PANIC("BundleStore::init called multiple times");
    }
    instance_ = new BundleStore(cfg);
    return instance_->bundles_.do_init(cfg, store);
}

//----------------------------------------------------------------------
bool
BundleStore::add(Bundle* bundle)
{
    bool ret = bundles_.add(bundle);
    if (ret) {
        total_size_ += bundle->durable_size();
    }
    return ret;
}

//----------------------------------------------------------------------
Bundle*
BundleStore::get(u_int32_t bundleid)
{
    return bundles_.get(bundleid);
}
    
//----------------------------------------------------------------------
bool
BundleStore::update(Bundle* bundle)
{
    return bundles_.update(bundle);
}

//----------------------------------------------------------------------
bool
BundleStore::del(Bundle* bundle)
{
    bool ret = bundles_.del(bundle->bundleid_);
    if (ret) {
        ASSERT(total_size_ >= bundle->durable_size());
        total_size_ -= bundle->durable_size();
    }
    return ret;
}

//----------------------------------------------------------------------
BundleStore::iterator*
BundleStore::new_iterator()
{
    return bundles_.new_iterator();
}
        
//----------------------------------------------------------------------
void
BundleStore::close()
{
    bundles_.close();
}


} // namespace dtn


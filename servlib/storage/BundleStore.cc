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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "BundleStore.h"
#include "bundling/Bundle.h"

#include <oasys/storage/DurableStore.h>

template <>
dtn::BundleStore* oasys::Singleton<dtn::BundleStore, false>::instance_ = 0;

namespace dtn {

bool BundleStore::using_aux_table_ = false;

//----------------------------------------------------------------------
BundleStore::BundleStore(const DTNStorageConfig& cfg)
    : cfg_(cfg),
      bundles_("BundleStore", "/dtn/storage/bundles",
               "bundle", "bundles"),
      payload_fdcache_("/dtn/storage/bundles/fdcache",
                       cfg.payload_fd_cache_size_),
#ifdef LIBODBC_ENABLED
      bundle_details_("BundleStoreExtra", "/dtn/storage/bundle_details",
               "BundleDetail", "bundles_aux"),
#endif
                       total_size_(0)
{
}

//----------------------------------------------------------------------
int
BundleStore::init(const DTNStorageConfig& cfg,
                  oasys::DurableStore*    store)
{
    int err;
    if (instance_ != NULL) {
        PANIC("BundleStore::init called multiple times");
    }
    instance_ = new BundleStore(cfg);
    err = instance_->bundles_.do_init(cfg, store);
#ifdef LIBODBC_ENABLED
    // Auxiliary tables only available with ODBC/SQL
    if ((err == 0) && store->aux_tables_available()) {
    	err = instance_->bundle_details_.do_init_aux(cfg, store);
    	if (err == 0) {
    		using_aux_table_ = true;
    		log_debug_p("/dtn/storage/bundle",
    				    "BundleStore::init initialised auxiliary bundle details table.");
    	}
    } else {
    	log_debug_p("/dtn/storage/bundle", "BundleStore::init skipping auxiliary table initialisation.");
    }
#endif
    return err;
}

//----------------------------------------------------------------------
bool
BundleStore::add(Bundle* bundle)
{
    bool ret = bundles_.add(bundle);

    if (ret) {
        total_size_ += bundle->durable_size();
#ifdef LIBODBC_ENABLED
        if (using_aux_table_) {
        	BundleDetail *details = new BundleDetail(bundle);
        	ret = bundle_details_.add(details);
        	delete details;
        }
#endif
    }
    return ret;
}

//----------------------------------------------------------------------
// All the information about the bundle is in the main bundles table.
// The auxiliary details table does not (currently) need to be accessed.
// NOTE (elwyn/20111231): The auxiliary table 'get' code is essentially
// untested.  The main purpose of the auxiliary table is to allow external
// processes to see the totality of the bundle cache, rather than accessing
// individual bundles, and/or having them delivered.  To this end the auxiliary
// table is 'write only' as far as DTN2 is concerned at present.
Bundle*
BundleStore::get(u_int32_t bundleid)
{
	return bundles_.get(bundleid);
}
    
//----------------------------------------------------------------------
// Whether update has to touch the auxiliary table depends on the items
// placed into the auxiliary details table.  In most cases it is expected
// that the items will be non-mutable over the life of the bundle, so it will
// not be necessary to update the auxiliary table.  The code below can be enabled
// if this situation changes.
bool
BundleStore::update(Bundle* bundle)
{
	int ret;

	ret = bundles_.update(bundle);

#ifdef LIBODBC_ENABLED
#if 0
    if ((ret == 0) && using_aux_table_) {
    	BundleDetail *details = new BundleDetail(bundle);
    	ret = bundle_details_.update(details);
    	delete details;
    }
#endif
#endif

    return ret;
}

//----------------------------------------------------------------------
// When an auxiliary table is configured (only in ODBC/SQL databases), deletion
// of corresponding rows from the auxiliary table is handled by triggers in the
// database.  No action is needed here to keep the tables consistent.
bool
BundleStore::del(Bundle* bundle)
{
    bool ret = bundles_.del(bundle->bundleid());
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
#ifdef LIBODBC_ENABLED
    // Auxiliary tables only available with ODBC/SQL
    if (using_aux_table_)
    {
    	bundle_details_.close();
    }
#endif
}


} // namespace dtn


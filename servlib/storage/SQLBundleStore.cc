/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if __SQL_ENABLED__

#include "SQLBundleStore.h"
#include "SQLStore.h"
#include "StorageConfig.h"
#include "bundling/Bundle.h"

/******************************************************************************
 *
 * SQLBundleStore
 *
 *****************************************************************************/

/**
 * Constructor.
 */

SQLBundleStore::SQLBundleStore(SQLImplementation* db, const char* table_name)
    : BundleStore()
{
    Bundle tmpobj(this);

    store_ = new SQLStore(table_name, db);
    store_->create_table(&tmpobj);
    store_->set_key_name("bundleid");
}

/**
 * Create a new bundle instance, then make a generic call into the
 * bundleStore to look up the bundle and fill in it's members if
 * found.
 */

Bundle*
SQLBundleStore::get(int bundle_id)
{
    Bundle* bundle = new Bundle();
    if (store_->get(bundle, bundle_id) != 0) {
        delete bundle;
        return NULL;
    }

    return bundle;
}

/**
 * Store the given bundle in the persistent store.
 */
bool
SQLBundleStore::insert(Bundle* bundle)
{
    return store_->insert(bundle) == 0;
}

/**
 * Update the given bundle in the persistent store.
 */
bool
SQLBundleStore::update(Bundle* bundle)
{
    return store_->update(bundle, bundle->bundleid_) == 0;
}

/**
 * Delete the given bundle from the persistent store.
 */
bool
SQLBundleStore::del(int bundle_id)
{
    return store_->del(bundle_id) == 0;
}



int 
SQLBundleStore::delete_expired(const time_t now) 
{
    const char* field = "expiration";
    StringBuffer query ;
    query.appendf("DELETE FROM %s WHERE %s > %lu", store_->table_name(), field, now);
    
    int retval = store_->exec_query(query.c_str());
    return retval;
}
     


bool
SQLBundleStore::is_custodian(int bundle_id) 
{
    NOTIMPLEMENTED ; 

    /**
     * One possible implementation. Currently b.is_custodian() 
     * function does not exist
     * Bundle b = get(bundle_id);
     * if (b == NULL) return 0;
     * return (b.is_custodian()) ;
     */
}

#endif /* __SQL_ENABLED__ */

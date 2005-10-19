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

#include <oasys/storage/DurableStore.h>
#include <oasys/storage/StorageConfig.h>
#include <oasys/serialize/TypeShims.h>

#include "BundleStore.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleEvent.h"

namespace dtn {

BundleStore* BundleStore::instance_;

static const char* BUNDLE_TABLE = "bundles";

BundleStore::BundleStore()
    : Logger("/storage/bundles"), store_(0)
{
}

int
BundleStore::do_init(const oasys::StorageConfig& cfg,
                     oasys::DurableStore*        store)
{
    int flags = 0;

    if (cfg.init_)
        flags |= oasys::DS_CREATE;
    int err = store->get_table(&store_, BUNDLE_TABLE, flags);

    if (err != 0) {
        log_err("error initializing bundle store");
        return err;
    }

    return 0;
}

bool
BundleStore::load()
{
    Bundle* bundle;
    int err;
    
    log_debug("Loading existing bundles from database.");

    // load existing stored bundles
    oasys::DurableIterator* iter = store_->iter();

    while (1) {
        oasys::IntShim key(-1);
        
        err = iter->next();
        if (err == oasys::DS_NOTFOUND)
        {
            break; // all done
        }
        else if (err != 0)
        {
            log_err("BundleStore::load: error in iterator next");
            return false;
        }
        
        
        err = iter->get(&key);
        if (err != 0)
        {
            log_err("BundleStore::load: error in iterator get");
            return false;
        }
        
        if (key.value() == -1)
        {
            log_err("BundleStore::load: error extracting key value");
            return false;
        }

        err = store_->get(key, &bundle);
        if (err != 0)
        {
            log_err("BundleStore::load: error retrieving bundle");
            return false;
        }

        ASSERT(bundle);
        bundle->payload_.init(&bundle->lock_, bundle->bundleid_);

        BundleDaemon::post(new BundleReceivedEvent(bundle, EVENTSRC_STORE));
    }

    delete iter;
    
    return true;
}

BundleStore::~BundleStore()
{
}

bool
BundleStore::add(Bundle* bundle)
{
    int err = store_->put(oasys::IntShim(bundle->bundleid_), bundle,
                          oasys::DS_CREATE | oasys::DS_EXCL);

    if (err == oasys::DS_EXISTS) {
        log_err("add bundle *%p: bundle already exists", bundle);
        return false;
    }

    if (err != 0) {
        PANIC("BundleStore::add_bundle(*%p): fatal database error", bundle);
    }
    
    return true;
}

bool
BundleStore::update(Bundle* bundle)
{
    int err = store_->put(oasys::IntShim(bundle->bundleid_), bundle, 0);

    if (err == oasys::DS_NOTFOUND) {
        log_err("update bundle *%p: bundle doesn't exist", bundle);
        return false;
    }
    
    if (err != 0) {
        PANIC("BundleStore::add_bundle(*%p): fatal database error", bundle);
    }

    return true;
}

bool
BundleStore::del(int bundle_id)
{
    int err = store_->del(oasys::IntShim(bundle_id));
    if (err != 0) {
        log_err("del bundle %d: %s", bundle_id,
                (err == oasys::DS_NOTFOUND) ?
                "bundle doesn't exist" : "unknown error");
        return false;
    }

    return true;
}


void
BundleStore::close()
{
    log_debug("closing bundle store");

    delete store_;
    store_ = NULL;
}

} // namespace dtn

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

#include "bundling/Bundle.h"
#include "BundleStore.h"
#include "PersistentStore.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleEvent.h"

namespace dtn {

BundleStore* BundleStore::instance_;

BundleStore::BundleStore(PersistentStore * store)
    : Logger("/storage/bundles")
{
    store_ = store;
}

bool
BundleStore::load()
{
    log_debug("Loading existing bundles from database.");

    // load existing stored bundles
    Bundle* bundle;
    std::vector<int> ids;
    std::vector<int>::iterator iter;

    store_->keys(&ids);

    for (iter = ids.begin(); iter != ids.end(); ++iter)
    {
        bundle = get(*iter);
        ASSERT(bundle);
        BundleDaemon::post(new BundleReceivedEvent(bundle, EVENTSRC_STORE));
    }

    return true;
}

BundleStore::~BundleStore()
{
}

Bundle*
BundleStore::get(int bundle_id)
{
    Bundle* bundle = new Bundle(bundle_id, this);
    if (store_->get(bundle, bundle_id) != 0) {
        delete bundle;
        return NULL;
    }

    return bundle;
}

bool
BundleStore::add(Bundle* bundle)
{
    return store_->add(bundle, bundle->bundleid_) == 0;
}

bool
BundleStore::update(Bundle* bundle)
{
    return store_->update(bundle, bundle->bundleid_) == 0;
}

bool
BundleStore::del(int bundle_id)
{
    return store_->del(bundle_id) == 0;
}


void
BundleStore::close()
{
    log_debug("closing bundle store");

    if (store_->close() != 0) {
        log_err("error closing bundle store");
    }
}

} // namespace dtn

/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
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

#include "APIRegistration.h"
#include "bundling/Bundle.h"
#include "bundling/BundleList.h"
#include "bundling/BundleMapping.h"

namespace dtn {

APIRegistration::APIRegistration(const oasys::Builder& builder)
    : Registration(builder)
{
    bundle_list_ = new BundleList(logpath_);
}
    
APIRegistration::APIRegistration(u_int32_t regid,
                                 const BundleTuplePattern& endpoint,
                                 failure_action_t action,
                                 time_t expiration,
                                 const std::string& script)
    : Registration(regid, endpoint, action, expiration, script)
{
    bundle_list_ = new BundleList(logpath_);
}

APIRegistration::~APIRegistration()
{
    delete bundle_list_;
}

void
APIRegistration::consume_bundle(Bundle* bundle, const BundleMapping* mapping)
{
    log_info("enqueue bundle id %d for delivery to %s",
             bundle->bundleid_, dest_str_.c_str());
    bundle_list_->push_back(bundle, mapping);

    // XXX/demmer this should handle the connected / disconnnected
    // action stuff
}

bool
APIRegistration::dequeue_bundle(Bundle* bundle, BundleMapping** mappingp)
{
    log_info("dequeue bundle id %d from %s",
             bundle->bundleid_, dest_str_.c_str());
    
    BundleMapping* mapping = bundle->get_mapping(bundle_list_);

    if (!mapping)
        return false;
    
    bundle_list_->erase(mapping->position_, mappingp);
    return true;
}

bool
APIRegistration::is_queued(Bundle* bundle)
{
    return (bundle->get_mapping(bundle_list_) != NULL);
}

} // namespace dtn

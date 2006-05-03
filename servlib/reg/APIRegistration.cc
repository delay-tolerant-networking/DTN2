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
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"

namespace dtn {

APIRegistration::APIRegistration(const oasys::Builder& builder)
    : Registration(builder)
{
    bundle_list_ = new BlockingBundleList(logpath_);
}
    
APIRegistration::APIRegistration(u_int32_t regid,
                                 const EndpointIDPattern& endpoint,
                                 failure_action_t action,
                                 u_int32_t expiration,
                                 const std::string& script)
    : Registration(regid, endpoint, action, expiration, script)
{
    bundle_list_ = new BlockingBundleList(logpath_);
}

APIRegistration::~APIRegistration()
{
    delete bundle_list_;
}

void
APIRegistration::deliver_bundle(Bundle* bundle)
{
    if (!active() && (failure_action_ == DROP)) {
        log_info("deliver_bundle: "
                 "dropping bundle id %d for passive registration %d (%s)",
                 bundle->bundleid_, regid_, endpoint_.c_str());
        
        // post an event saying we "delivered" it
        BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
    }

    if (!active() && (failure_action_ == EXEC)) {
        // this sure seems like a security hole, but what can you
        // do -- it's in the spec
        log_info("deliver_bundle: "
                 "running script '%s' for registration %d (%s)",
                 script_.c_str(), regid_, endpoint_.c_str());
        
        system(script_.c_str());
        // fall through
    }

    log_info("deliver_bundle: queuing bundle id %d for %s delivery to %s",
             bundle->bundleid_,
             active() ? "active" : "deferred",
             endpoint_.c_str());

    if (BundleDaemon::instance()->params_.test_permuted_delivery_) {
        bundle_list_->insert_random(bundle);
    } else {
        bundle_list_->push_back(bundle);
    }
}


} // namespace dtn

/*
 *    Copyright 2005-2006 Intel Corporation
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
                 bundle->bundleid(), regid_, endpoint_.c_str());
        
        // post an event saying we "delivered" it
        BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
        return;
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
             bundle->bundleid(),
             active() ? "active" : "deferred",
             endpoint_.c_str());

    if (BundleDaemon::instance()->params_.test_permuted_delivery_) {
        bundle_list_->insert_random(bundle);
    } else {
        bundle_list_->push_back(bundle);
    }
}


} // namespace dtn

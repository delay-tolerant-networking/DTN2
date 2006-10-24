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


#include "BundleDaemon.h"
#include "BundleEvent.h"
#include "ExpirationTimer.h"

namespace dtn {

ExpirationTimer::ExpirationTimer(Bundle* bundle)
    : bundleref_(bundle, "expiration timer")
{
}

void
ExpirationTimer::timeout(const struct timeval& now)
{
    (void)now;
    oasys::ScopeLock l(&bundleref_->lock_, "ExpirationTimer::timeout");

    // null out the pointer to ourself in the bundle class
    bundleref_->expiration_timer_ = NULL;
    
    // post the expiration event
    BundleDaemon::post_at_head(new BundleExpiredEvent(bundleref_.object()));

    // clean ourselves up
    delete this;
}

} // namespace dtn

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


#include <oasys/util/StringUtils.h>

#include "FloodConsumer.h"
#include "reg/Registration.h"

namespace dtnsim {

FloodConsumer::FloodConsumer(u_int32_t regid,
                             const EndpointIDPattern& endpoint)
    : Registration(regid, endpoint, Registration::ABORT)
{
    set_active(true);

    log_info("FLOOD_CONSUMER : adding registration at endpoint:%s",
             endpoint.c_str());
}

void
FloodConsumer::enqueue_bundle(Bundle *bundle)
{
    log_info("FLOOD_CONSUMER : consuming bundle id:%d",bundle->bundleid_);

    //add stats
    //router
}

void
FloodConsumer::set_router(BundleRouter *router)
{
    router_ = router;
}

} // namespace dtnsim

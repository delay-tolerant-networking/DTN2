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
#  include <config.h>
#endif

#include <oasys/util/ScratchBuffer.h>

#include "PingRegistration.h"
#include "RegistrationTable.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

PingRegistration::PingRegistration(const EndpointID& eid)
    : Registration(PING_REGID, eid, Registration::DEFER, 0)
{
    logpathf("/dtn/reg/ping");
    set_active(true);
}

void
PingRegistration::deliver_bundle(Bundle* bundle)
{
    size_t payload_len = bundle->payload_.length();
    log_debug("%zu byte ping from %s",
              payload_len, bundle->source_.c_str());
    
    Bundle* reply = new Bundle();
    reply->source_.assign(endpoint_);
    reply->dest_.assign(bundle->source_);
    reply->replyto_.assign(EndpointID::NULL_EID());
    reply->custodian_.assign(EndpointID::NULL_EID());
    reply->expiration_ = bundle->expiration_;

    reply->payload_.set_length(payload_len);
    reply->payload_.write_data(&bundle->payload_, 0, payload_len, 0);
    
    BundleDaemon::post(new BundleReceivedEvent(reply, EVENTSRC_ADMIN));
    BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
}


} // namespace dtn

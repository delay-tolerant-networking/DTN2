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
    reply->source_.assign(bundle->dest_);
    reply->dest_.assign(bundle->source_);
    reply->replyto_.assign(EndpointID::NULL_EID());
    reply->custodian_.assign(EndpointID::NULL_EID());
    reply->expiration_ = bundle->expiration_;

    reply->payload_.set_length(payload_len);
    reply->payload_.write_data(&bundle->payload_, 0, payload_len, 0);
    reply->payload_.close_file();
    
    BundleDaemon::post(new BundleReceivedEvent(reply, EVENTSRC_ADMIN, payload_len));
    BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
}


} // namespace dtn

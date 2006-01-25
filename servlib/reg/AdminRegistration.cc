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

#include "AdminRegistration.h"
#include "RegistrationTable.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "bundling/CustodySignal.h"
#include "routing/BundleRouter.h"

namespace dtn {

AdminRegistration::AdminRegistration()
    : Registration(ADMIN_REGID,
                   BundleDaemon::instance()->local_eid(),
                   Registration::DEFER,
                   0)
{
    logpathf("/reg/admin");
    BundleDaemon::post(new RegistrationAddedEvent(this));
}

void
AdminRegistration::deliver_bundle(Bundle* bundle)
{
    u_char typecode;

    size_t payload_len = bundle->payload_.length();
    oasys::ScratchBuffer<u_char*, 256> scratch(payload_len);
    const u_char* payload_buf = 
        bundle->payload_.read_data(0, payload_len, scratch.buf(payload_len));
    
    log_debug("got %u byte bundle", (u_int)payload_len);
        
    if (payload_len == 0) {
        log_err("admin registration got 0 byte bundle *%p", bundle);
        goto done;
    }

    /*
     * As outlined in the bundle specification, the first four bits of
     * all administrative bundles hold the type code, with the
     * following values:
     *
     * 0x1     - bundle status report
     * 0x2     - custodial signal
     * 0x3     - echo request
     * 0x4     - null request
     * (other) - reserved
     */
    typecode = payload_buf[0] >> 4;
    
    switch(typecode) {
    case BundleProtocol::ADMIN_STATUS_REPORT:
    {
        log_err("status report *%p received at admin registration", bundle);
        break;
    }
    
    case BundleProtocol::ADMIN_CUSTODY_SIGNAL:
    {
        log_info("ADMIN_CUSTODY_SIGNAL *%p received", bundle);
        CustodySignal::data_t data;
        
        bool ok = CustodySignal::parse_custody_signal(&data, payload_buf, payload_len);
        if (!ok) {
            log_err("malformed custody signal *%p", bundle);
            break;
        }

        BundleDaemon::post(new CustodySignalEvent(data));

        break;
    }
    case BundleProtocol::ADMIN_ECHO:
    {
        log_info("ADMIN_ECHO from %s", bundle->source_.c_str());

        Bundle* reply = new Bundle();
        reply->source_.assign(bundle->dest_);
        reply->dest_.assign(bundle->source_);
        reply->replyto_.assign(EndpointID::NULL_EID());
        reply->custodian_.assign(EndpointID::NULL_EID());
        reply->expiration_ = bundle->expiration_;

        // we impose an arbitrary cap of 1K on the payload data to be echoed
        size_t payload_len = bundle->payload_.length();

        u_char buf[1024];
        const u_char* bp = bundle->payload_.read_data(0, payload_len, buf);
        reply->payload_.set_data(bp, payload_len);
        
        BundleDaemon::post(new BundleReceivedEvent(reply, EVENTSRC_ADMIN, payload_len));
        
        break;
    }   
    case BundleProtocol::ADMIN_NULL:
    {
        log_info("ADMIN_NULL from %s", bundle->source_.c_str());
        break;
    }
        
    default:
        log_warn("unexpected admin bundle with type 0x%x *%p",
                 typecode, bundle);
    }    

 done:
    BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
}


} // namespace dtn

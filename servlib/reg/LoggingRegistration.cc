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

#include <oasys/util/StringUtils.h>

#include "LoggingRegistration.h"
#include "Registration.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleList.h"

namespace dtn {

LoggingRegistration::LoggingRegistration(const BundleTuplePattern& endpoint)
    : Registration(endpoint, Registration::ABORT)
{
    logpathf("/registration/logging/%d", regid_);
    set_active(true);
    
    log_info("new logging registration on endpoint %s", endpoint.c_str());
}

void
LoggingRegistration::run()
{
    Bundle* b;
    while (1) {
        b = bundle_list_->pop_blocking();

        log_info("BUNDLE: id %d priority %s dopts: [%s %s %s %s %s]",
                 b->bundleid_, Bundle::prioritytoa(b->priority_),
                 b->custreq_ ? "custreq" : "",
                 b->custody_rcpt_ ? "custody_rcpt" : "",
                 b->recv_rcpt_ ? "recv_rcpt" : "",
                 b->fwd_rcpt_ ? "fwd_rcpt" : "",
                 b->return_rcpt_ ? "return_rcpt" : "");

        log_info("        source:    %s", b->source_.c_str());
        log_info("        dest:      %s", b->dest_.c_str());
        log_info("        replyto:   %s", b->replyto_.c_str());
        log_info("        custodian: %s", b->custodian_.c_str());
        log_info("        created %u.%u, expiration %d",
                 (u_int32_t)b->creation_ts_.tv_sec,
                 (u_int32_t)b->creation_ts_.tv_usec,
                 b->expiration_);

        size_t len = 128;
        size_t payload_len = b->payload_.length();
        if (payload_len < len) {
            len = payload_len;
        }

        u_char payload_buf[payload_len];
        const u_char* data = b->payload_.read_data(0, len, payload_buf);

	if (oasys::str_isascii(data, len)) {
            log_info("        payload (ascii): length %d '%.*s'",
                     payload_len, (int)len, data);
        } else {
            std::string hex;
            oasys::hex2str(&hex, data, len);
            len *= 2;
            if (len > 128)
                len = 128;
            log_info("        payload (binary): length %d %.*s",
                     payload_len, (int)len, hex.data());
        }

        BundleForwarder::post(
            new BundleTransmittedEvent(b, this, b->payload_.length(), true));
        
        b->del_ref("LoggingRegistration");
    }
}

} // namespace dtn

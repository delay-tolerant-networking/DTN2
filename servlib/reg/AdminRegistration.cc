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

#include <oasys/util/StringBuffer.h>

#include "AdminRegistration.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "routing/BundleRouter.h"

namespace dtn {

AdminRegistration::AdminRegistration()
    : Registration(ADMIN_REGID, BundleRouter::local_tuple_, ABORT)
{
    logpathf("/reg/admin");
}

void
AdminRegistration::enqueue_bundle(Bundle* bundle,
                                  const BundleMapping* mapping)
{
    u_char typecode;
    
    size_t payload_len = bundle->payload_.length();
    oasys::StringBuffer payload_buf(payload_len);
    log_debug("got %d byte bundle", payload_len);

    if (payload_len == 0) {
        log_err("admin registration got 0 byte bundle *%p", bundle);
        goto done;
    }

    /*
     * As outlined in the bundle specification, the first byte of all
     * administrative bundles is a type code, with the following
     * values:
     *
     * 0x1     - bundle status report
     * 0x2     - custodial signal
     * 0x3     - echo request
     * 0x4     - null request
     * (other) - reserved
     */
    typecode = *(bundle->payload_.read_data(0, 1, (u_char*)payload_buf.data()));

    switch(typecode) {
    case BundleProtocol::ADMIN_STATUS_REPORT:
        PANIC("status report not implemented yet");
        break;

    case BundleProtocol::ADMIN_CUSTODY_SIGNAL:
        PANIC("custody signal not implemented yet");
        break;

    case BundleProtocol::ADMIN_ECHO:
        log_info("ADMIN_ECHO from %s", bundle->source_.c_str());

        // XXX/demmer implement the echo
        break;
        
    case BundleProtocol::ADMIN_NULL:
        log_info("ADMIN_NULL from %s", bundle->source_.c_str());
        break;
        
    default:
        log_warn("unexpected admin bundle with type 0x%x *%p",
                 typecode, bundle);
    }    

 done:
    BundleDaemon::post(
        new BundleTransmittedEvent(bundle, this, payload_len, true));
}

                   
/**
 * Attempt to remove the given bundle from the queue.
 *
 * @return true if the bundle was dequeued, false if not.
 */
bool
AdminRegistration::dequeue_bundle(Bundle* bundle,
                                  BundleMapping** mappingp)
{
    // since there's no queue, we can't ever dequeue something
    return false;
}


/**
 * Check if the given bundle is already queued on this consumer.
 */
bool
AdminRegistration::is_queued(Bundle* bundle)
{
    return false;
}

} // namespace dtn

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

#include "SimRegistration.h"
#include "Topology.h"
#include "bundling/Bundle.h"

using namespace dtn;

namespace dtnsim {

SimRegistration::SimRegistration(Node* node, const BundleTuple& demux_tuple)
    : Registration(Topology::next_regid(), demux_tuple, ABORT)
{
    logpathf("/reg/%s/%d", node->name(), regid_);

    log_debug("new sim registration");
}

void
SimRegistration::enqueue_bundle(Bundle* bundle,
                                const BundleMapping* mapping)
{
    size_t payload_len = bundle->payload_.length();
    log_debug("got %d byte bundle", payload_len);
}
                   
/**
 * Attempt to remove the given bundle from the queue.
 *
 * @return true if the bundle was dequeued, false if not.
 */
bool
SimRegistration::dequeue_bundle(Bundle* bundle,
                                  BundleMapping** mappingp)
{
    // since there's no queue, we can't ever dequeue something
    return false;
}


/**
 * Check if the given bundle is already queued on this consumer.
 */
bool
SimRegistration::is_queued(Bundle* bundle)
{
    return false;
}

} // namespace dtnsim

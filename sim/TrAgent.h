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
#ifndef _TR_AGENT_H
#define _TR_AGENT_H

#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Log.h>

#include "SimEvent.h"
#include "SimEventHandler.h"
#include "naming/EndpointID.h"

namespace dtnsim {
class Node;

class TrAgent : public SimEventHandler, public oasys::Logger {
public:
    static TrAgent* init(Node* node, double start_time,
                         const EndpointID& src, const EndpointID& dst,
                         int argc, const char** argv);

    virtual ~TrAgent() {}

    void process(SimEvent *e);

private:
    TrAgent(Node* node, const EndpointID& src, const EndpointID& dst);
    
    void send_bundle();

    Node* node_;	///< node where the traffic is injected
    EndpointID src_;	///< source eid
    EndpointID dst_;	///< destination eid
    int size_;		///< size of each message
    int reps_;		///< total number of reps/batches
    int batch_;		///< no of messages in each batch
    double interval_;	///< time gap between two batches
};

} // namespace dtnsim

#endif /* _TR_AGENT_H */

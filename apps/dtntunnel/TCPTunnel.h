/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2006 Intel Corporation. All rights reserved. 
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
#ifndef _TCPTUNNEL_H_
#define _TCPTUNNEL_H_

#include <dtn_api.h>

#include <oasys/debug/Log.h>
#include <oasys/io/TCPClient.h>
#include <oasys/io/TCPServer.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/thread/Thread.h>
#include <oasys/util/ExpandableBuffer.h>

#include "IPTunnel.h"

namespace dtntunnel {

/**
 * Class to manage TCP <-> DTN tunnels.
 */
class TCPTunnel : public IPTunnel {
public:
    /// Constructor
    TCPTunnel();
    
    /// Add a new listening to from the given listening
    /// address/port to the given remote address/port
    void add_listener(in_addr_t listen_addr, u_int16_t listen_port,
                      in_addr_t remote_addr, u_int16_t remote_port);
    
    /// Handle a newly arriving bundle
    void handle_bundle(dtn::APIBundle* bundle);

protected:

    /// Add a new proxy definition
    void add_proxy(in_addr_t listen_addr, u_int16_t listen_port,
                   in_addr_t remote_addr, u_int16_t remote_port);

    
    /// Helper class to accept incoming TCP connections
    class Listener : public oasys::TCPServerThread {
    public:
        Listener(TCPTunnel* t,
                 in_addr_t listen_addr, u_int16_t listen_port,
                 in_addr_t remote_addr, u_int16_t remote_port);
        
        void accepted(int fd, in_addr_t addr, u_int16_t port);
    };

    /// Helper class to handle an actively proxied connection
    class Connection : public oasys::Thread, public oasys::Logger {
    public:
        /// Constructor called to initiate a connection due to an
        /// arriving bundle request
        Connection(in_addr_t addr, u_int16_t port);

        /// Constructor called when a new connection was accepted
        Connection(int fd, in_addr_t addr, u_int16_t port);
        
    protected:
        /// virtual run method
        void run();

        /// The tcp socket
        oasys::TCPClient* sock_;
    };
};

} // namespace dtntunnel

#endif /* _TCPTUNNEL_H_ */

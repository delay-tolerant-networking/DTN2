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

#include <oasys/io/NetUtils.h>

#include "DTNTunnel.h"
#include "UDPTunnel.h"

namespace dtntunnel {

//----------------------------------------------------------------------
UDPTunnel::UDPTunnel()
    : IPTunnel("UDPTunnel", "/dtntunnel/udp"),
      sock_("/dtntunnel/udp/sock")
{
    sock_.init_socket();
}

//----------------------------------------------------------------------
void
UDPTunnel::add_listener(in_addr_t listen_addr, u_int16_t listen_port,
                        in_addr_t remote_addr, u_int16_t remote_port)
{
    // the object will delete itself when the thread exits
    new Listener(listen_addr, listen_port, remote_addr, remote_port);
}
    
//----------------------------------------------------------------------
void
UDPTunnel::handle_bundle(dtn::APIBundle* bundle)
{
    DTNTunnel::BundleHeader hdr;
    memcpy(&hdr, bundle->payload_.buf(), sizeof(hdr));
    hdr.remote_port_ = htons(hdr.remote_port_);

    char* bp = bundle->payload_.buf() + sizeof(hdr);
    int  len = bundle->payload_.len() - sizeof(hdr);
    
    int cc = sock_.sendto(bp, len,
                           0, hdr.remote_addr_, hdr.remote_port_);
    if (cc != len) {
        log_err("error sending packet to %s:%d: %s",
                intoa(hdr.remote_addr_), hdr.remote_port_, strerror(errno));
    } else {
        log_info("sent %d byte packet to %s:%d",
                 bundle->payload_.len() - sizeof(hdr),
                 intoa(hdr.remote_addr_), hdr.remote_port_);
    }

    delete bundle;
}

//----------------------------------------------------------------------
UDPTunnel::Listener::Listener(in_addr_t listen_addr, u_int16_t listen_port,
                              in_addr_t remote_addr, u_int16_t remote_port)
    : Thread("UDPTunnel::Listener", DELETE_ON_EXIT),
      Logger("UDPTunnel::Listener", "/dtntunnel/udp/listener"), 
      sock_("/dtntunnel/udp/listener/sock"),
      listen_addr_(listen_addr),
      listen_port_(listen_port),
      remote_addr_(remote_addr),
      remote_port_(remote_port)
{
    start();
}

//----------------------------------------------------------------------
void
UDPTunnel::Listener::run()
{
    DTNTunnel* tunnel = DTNTunnel::instance();
    int ret = sock_.bind(listen_addr_, listen_port_);
    if (ret != 0) {
        log_err("can't bind to %s:%u", intoa(listen_addr_), listen_port_);
        return; // die
    }

    DTNTunnel::BundleHeader hdr;
    hdr.protocol_    = IPPROTO_UDP;
    hdr.seqno_       = 0;
    hdr.client_addr_ = listen_addr_;
    hdr.client_port_ = htons(listen_port_);
    hdr.remote_addr_ = remote_addr_;
    hdr.remote_port_ = htons(remote_port_);
    
    while (1) {
        int len = sock_.recv(recv_buf_, sizeof(recv_buf_), 0);
        if (len <= 0) {
            log_err("error reading from socket: %s", strerror(errno));
            return; // die
        }
        
        log_info("got %d byte packet", len);

        dtn::APIBundle* b = new dtn::APIBundle();
        char* bp = b->payload_.buf(sizeof(hdr) + len);
        memcpy(bp, &hdr, sizeof(hdr));
        memcpy(bp + sizeof(hdr), recv_buf_, len);
        b->payload_.set_len(sizeof(hdr) + len);
        
        tunnel->send_bundle(b, tunnel->dest_eid());
    }
}

} // namespace dtntunnel

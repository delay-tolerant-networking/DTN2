/*
 *    Copyright 2006 Intel Corporation
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
#  include <dtn-config.h>
#endif

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
        log_debug("sent %zu byte packet to %s:%d",
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
        
        log_debug("got %d byte packet", len);

        dtn::APIBundle* b = new dtn::APIBundle();
        char* bp = b->payload_.buf(sizeof(hdr) + len);
        memcpy(bp, &hdr, sizeof(hdr));
        memcpy(bp + sizeof(hdr), recv_buf_, len);
        b->payload_.set_len(sizeof(hdr) + len);
        
        if (tunnel->send_bundle(b, tunnel->dest_eid()) != DTN_SUCCESS)
            exit(1);
    }
}

} // namespace dtntunnel

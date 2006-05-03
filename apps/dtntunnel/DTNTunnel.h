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
#ifndef _DTNTUNNEL_H_
#define _DTNTUNNEL_H_

#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include <dtn_api.h>
#include <APIBundleQueue.h>
#include <oasys/debug/Log.h>
#include <oasys/thread/Mutex.h>
#include <oasys/util/Singleton.h>

namespace dtntunnel {

class TCPTunnel;
class UDPTunnel;

/**
 * Main wrapper class for the DTN Tunnel.
 */
class DTNTunnel : public oasys::Logger,
                  public oasys::Singleton<DTNTunnel>
{
public:
    /// Constructor
    DTNTunnel();

    /// Struct to encapsulate the header sent with each tunneled
    /// bundle. Note that since it is declared as a packed struct, it
    /// can be sent over the wire as-is.
    ///
    /// XXX/demmer if this is used for non-IP tunnels, the address
    /// fields will need to be union'd or something like that
    struct BundleHeader {
        BundleHeader() {} 

        BundleHeader(u_int8_t  protocol,
                     u_int32_t seqno,
                     u_int32_t client_addr,
                     u_int32_t remote_addr,
                     u_int16_t client_port,
                     u_int16_t remote_port)
            : protocol_(protocol),
              seqno_(seqno),
              client_addr_(client_addr),
              remote_addr_(remote_addr),
              client_port_(client_port),
              remote_port_(remote_port)
        {
        }

        u_int8_t  protocol_;
        u_int32_t seqno_;
        u_int32_t client_addr_;
        u_int32_t remote_addr_;
        u_int16_t client_port_;
        u_int16_t remote_port_;
                             
    } __attribute__((packed));

    /// Hook for various tunnel classes to send a bundle. Assumes
    /// ownership of the passed-in bundle
    int send_bundle(dtn::APIBundle* bundle, dtn_endpoint_id_t* dest_eid);

    /// Called for arriving bundles
    int handle_bundle(dtn_bundle_spec_t* spec,
                      dtn_bundle_payload_t* payload);

    /// Main application loop
    int main(int argc, char* argv[]);

    /// Accessors
    u_int max_size()              { return max_size_; }
    dtn_endpoint_id_t* dest_eid() { return &dest_eid_; }

protected:
    std::string         loglevelstr_;
    oasys::log_level_t  loglevel_;
    std::string         logfile_;

    UDPTunnel*          udptunnel_;
    TCPTunnel*          tcptunnel_;

    dtn_handle_t 	recv_handle_;
    dtn_handle_t 	send_handle_;
    oasys::Mutex	send_lock_;
    bool                listen_;
    dtn_endpoint_id_t 	local_eid_;
    dtn_endpoint_id_t 	dest_eid_;
    u_int		expiration_;
    bool                tcp_;
    bool                udp_;
    in_addr_t		local_addr_;
    u_int16_t		local_port_;
    in_addr_t		remote_addr_;
    u_int16_t		remote_port_;
    u_int		max_size_;

    void init_log();
    void init_tunnel();
    void init_registration();
    void get_options(int argc, char* argv[]);
};

} // namespace dtntunnel

#endif /* _DTNTUNNEL_H_ */

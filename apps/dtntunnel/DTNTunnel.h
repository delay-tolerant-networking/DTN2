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
        BundleHeader()
        {
            memset(this, 0, sizeof(BundleHeader));
        }
        
        BundleHeader(u_int8_t  protocol,
                     u_int8_t  eof,
                     u_int32_t seqno,
                     u_int32_t client_addr,
                     u_int32_t remote_addr,
                     u_int16_t client_port,
                     u_int16_t remote_port)
            : protocol_(protocol),
              eof_(eof),
              seqno_(seqno),
              client_addr_(client_addr),
              remote_addr_(remote_addr),
              client_port_(client_port),
              remote_port_(remote_port)
        {
        }

        u_int8_t  protocol_;
        u_int8_t  eof_;
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
    u_int delay()                 { return delay_; }
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
    bool		custody_;
    u_int		expiration_;
    bool                tcp_;
    bool                udp_;
    in_addr_t		local_addr_;
    u_int16_t		local_port_;
    in_addr_t		remote_addr_;
    u_int16_t		remote_port_;
    u_int		delay_;
    u_int		max_size_;

    void init_log();
    void init_tunnel();
    void init_registration();
    void get_options(int argc, char* argv[]);
};

} // namespace dtntunnel

#endif /* _DTNTUNNEL_H_ */

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

#include <map>

#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include <dtn_api.h>
#include <APIBundleQueue.h>
#include <oasys/debug/Log.h>
#include <oasys/thread/Mutex.h>
#include <oasys/util/App.h>
#include <oasys/util/Singleton.h>

namespace dtntunnel {

class TCPTunnel;
class UDPTunnel;

/**
 * Main wrapper class for the DTN Tunnel.
 */
class DTNTunnel : public oasys::App,
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
                     u_int32_t connection_id,
                     u_int32_t seqno,
                     u_int32_t client_addr,
                     u_int32_t remote_addr,
                     u_int16_t client_port,
                     u_int16_t remote_port)
            : protocol_(protocol),
              eof_(eof),
              connection_id_(connection_id),
              seqno_(seqno),
              client_addr_(client_addr),
              remote_addr_(remote_addr),
              client_port_(client_port),
              remote_port_(remote_port)
        {
        }

        u_int8_t  protocol_;
        u_int8_t  eof_;
        u_int32_t connection_id_;
        u_int32_t seqno_;
        u_int32_t client_addr_;
        u_int32_t remote_addr_;
        u_int16_t client_port_;
        u_int16_t remote_port_;
                             
    } __attribute__((packed));

    /// Hook for various tunnel classes to send a bundle. Assumes
    /// ownership of the passed-in bundle
    ///
    /// @return DTN_SUCCESS on success, a DTN_ERRNO value on error
    int send_bundle(dtn::APIBundle* bundle, dtn_endpoint_id_t* dest_eid);

    /// Called for arriving bundles
    int handle_bundle(dtn_bundle_spec_t* spec,
                      dtn_bundle_payload_t* payload);

    /// Main application loop
    int main(int argc, char* argv[]);

    /// Virtual from oasys::App
    void fill_options();
    void validate_options(int argc, char* const argv[], int remainder);

    /// Get destination eid from the dest_eid_table
    void get_dest_eid(in_addr_t remote_addr, dtn_endpoint_id_t* dest_eid);

    /// Accessors
    u_int max_size()              { return max_size_; }
    u_int delay()                 { return delay_; }
    u_int delay_set()             { return delay_set_; }
    bool reorder_udp()            { return reorder_udp_; }
    bool transparent()            { return transparent_; }
    dtn_endpoint_id_t* dest_eid() { return &dest_eid_; }

protected:
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
    bool                reorder_udp_;
    in_addr_t		local_addr_;
    u_int16_t		local_port_;
    in_addr_t		remote_addr_;
    u_int16_t		remote_port_;
    u_int		delay_;
    bool		delay_set_;
    u_int		max_size_;
    std::string	        tunnel_spec_;
    bool	        tunnel_spec_set_;
    bool		transparent_;

    /// Helper struct for network IP address in CIDR notation
    struct CIDR {
	CIDR()
	    : addr_(0),
	      bits_(0) {}
	
	CIDR(u_int32_t addr, u_int16_t bits)
	    : addr_(addr),
	      bits_(bits) {}

        bool operator<(const CIDR& other) const
        {
            #define COMPARE(_x) if (_x != other._x) return _x < other._x;
            COMPARE(addr_);
            #undef COMPARE

	    return bits_ < other.bits_;
        }

	u_int32_t addr_;
	u_int16_t bits_;
    };

    /// Table for IP network address <-> EID matching
    typedef std::map<DTNTunnel::CIDR, char*> NetworkEidTable;
    NetworkEidTable dest_eid_table_;

    void init_tunnel();
    void init_registration();
    void load_dest_eid_table(char* filename);
};

} // namespace dtntunnel

#endif /* _DTNTUNNEL_H_ */

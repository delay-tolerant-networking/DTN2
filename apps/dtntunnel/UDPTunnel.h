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

#ifndef _UDPTUNNEL_H_
#define _UDPTUNNEL_H_

#include <dtn_api.h>

#include <oasys/debug/Log.h>
#include <oasys/io/UDPClient.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/thread/Thread.h>
#include <oasys/util/ExpandableBuffer.h>

#include "IPTunnel.h"

namespace dtntunnel {

class DTNTunnel;

class UDPTunnel : public IPTunnel {
public:
    /// Constructor
    UDPTunnel();

    /// Add a new listener
    void add_listener(in_addr_t listen_addr, u_int16_t listen_port,
                      in_addr_t remote_addr, u_int16_t remote_port);
    
    /// Handle a newly arriving bundle
    void handle_bundle(dtn::APIBundle* bundle);

protected:
    /// Sender socket
    oasys::UDPClient sock_;
    
    /// Helper class to handle a proxied UDP port
    class Listener : public oasys::Thread, public oasys::Logger {
    public:
        /// Constructor
        Listener(in_addr_t listen_addr, u_int16_t listen_port,
                 in_addr_t remote_addr, u_int16_t remote_port);
        
    protected:
        /// Main listen loop
        void run();
        
        /// Receiver socket
        oasys::UDPClient sock_;

        /// Static receiving buffer
        char recv_buf_[65536];
        
        /// @{
        /// Proxy parameters
        in_addr_t listen_addr_;
        u_int16_t listen_port_;
        in_addr_t remote_addr_;
        u_int16_t remote_port_;
        /// @}
    };
    
};

} // namespace dtntunnel

#endif /* _UDPTUNNEL_H_ */

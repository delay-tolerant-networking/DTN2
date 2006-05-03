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

#include <map>
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
    /// Helper class to accept incoming TCP connections
    class Listener : public oasys::TCPServerThread {
    public:
        Listener(TCPTunnel* t,
                 in_addr_t listen_addr, u_int16_t listen_port,
                 in_addr_t remote_addr, u_int16_t remote_port);
        
        void accepted(int fd, in_addr_t addr, u_int16_t port);

    protected:
        TCPTunnel* tcptun_;

        /// @{
        /// Proxy parameters
        in_addr_t listen_addr_;
        u_int16_t listen_port_;
        in_addr_t remote_addr_;
        u_int16_t remote_port_;
        /// @}
    };

    /// Helper class to handle an actively proxied connection
    class Connection : public oasys::Thread, public oasys::Logger {
    public:
        /// Constructor called to initiate a connection due to an
        /// arriving bundle request
        Connection(TCPTunnel* t, dtn_endpoint_id_t* dest_eid,
                   in_addr_t client_addr, u_int16_t client_port,
                   in_addr_t remote_addr, u_int16_t remote_port);

        /// Constructor called when a new connection was accepted
        Connection(TCPTunnel* t, dtn_endpoint_id_t* dest_eid, int fd,
                   in_addr_t client_addr, u_int16_t client_port,
                   in_addr_t remote_addr, u_int16_t remote_port);

        /// Destructor
        ~Connection();
        
        /// Handle a newly arriving bundle
        void handle_bundle(dtn::APIBundle* bundle);
        
    protected:
        friend class TCPTunnel;
        
        /// virtual run method
        void run();

        /// The tcp tunnel object
        TCPTunnel* tcptun_;
        
        /// The tcp socket
        oasys::TCPClient sock_;

        /// Queue for bundles on this connection
        dtn::APIBundleQueue queue_;

        /// Table for out-of-order bundles
        typedef std::map<u_int32_t, dtn::APIBundle*> ReorderTable;
        ReorderTable reorder_table_;

        /// Running sequence number counter
        u_int32_t next_seqno_;

        /// Parameters for the connection
        dtn_endpoint_id_t dest_eid_;
        in_addr_t         client_addr_;
        u_int16_t         client_port_;
        in_addr_t         remote_addr_;
        u_int16_t         remote_port_;
    };

    /// Hook called by the listener when a new connection comes in
    void new_connection(Connection* c);

    /// Hook called when a new connection dies
    void kill_connection(Connection* c);

    /// Helper struct used as the index key into the connection table
    struct ConnKey {
        ConnKey()
            : client_addr_(INADDR_NONE), client_port_(0),
              remote_addr_(INADDR_NONE), remote_port_(0) {}

        ConnKey(in_addr_t client_addr, u_int16_t client_port,
                in_addr_t remote_addr, u_int16_t remote_port)
            : client_addr_(client_addr),
              client_port_(client_port),
              remote_addr_(remote_addr),
              remote_port_(remote_port) {}

        bool operator<(const ConnKey& other) const {
            return ((client_addr_ < other.client_addr_) ||
                    (client_port_ < other.client_port_) ||
                    (remote_addr_ < other.remote_addr_) ||
                    (remote_port_ < other.remote_port_));
        }
        
        in_addr_t client_addr_;
        u_int16_t client_port_;
        in_addr_t remote_addr_;
        u_int16_t remote_port_;
    };

    /// Table of connection classes indexed by the remote address/port
    typedef std::map<ConnKey, Connection*> ConnTable;
    ConnTable connections_;

    /// Lock to protect the connections table
    oasys::SpinLock lock_;
};

} // namespace dtntunnel

#endif /* _TCPTUNNEL_H_ */

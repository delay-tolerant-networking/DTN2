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
#ifndef _TCP_CONVERGENCE_LAYER_H_
#define _TCP_CONVERGENCE_LAYER_H_

#include <oasys/io/TCPClient.h>
#include <oasys/io/TCPServer.h>

#include "StreamConvergenceLayer.h"

namespace dtn {

/**
 * The TCP Convergence Layer.
 */
class TCPConvergenceLayer : public StreamConvergenceLayer {
public:
    /**
     * Current version of the protocol.
     */
    static const u_int8_t TCPCL_VERSION = 0x03;

    /**
     * Default port used by the tcp cl. XXX/demmer change this value
     */
    static const u_int16_t TCPCL_DEFAULT_PORT = 5000;
    
    /**
     * Constructor.
     */
    TCPConvergenceLayer();

    /// @{ Virtual from ConvergenceLayer
    bool interface_up(Interface* iface, int argc, const char* argv[]);
    bool interface_down(Interface* iface);
    void dump_interface(Interface* iface, oasys::StringBuffer* buf);
    /// @}

    /**
     * Tunable link parameter structure.
     */
    class TCPLinkParams : public StreamLinkParams {
    public:
        in_addr_t local_addr_;		///< Local address to bind to
        in_addr_t remote_addr_;		///< Peer address used for rcvr-connect
        u_int16_t remote_port_;		///< Peer port used for rcvr-connect

    protected:
        // See comment in LinkParams for why this is protected
        TCPLinkParams(bool init_defaults);
        friend class TCPConvergenceLayer;
    };

    /**
     * Default link parameters.
     */
    static TCPLinkParams default_link_params_;

protected:
    /// @{ Virtual from ConvergenceLayer
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp);
    void dump_link(Link* link, oasys::StringBuffer* buf);
    /// @}
    
    /// @{ Virtual from ConnectionConvergenceLayer
    virtual LinkParams* new_link_params();
    virtual bool parse_link_params(LinkParams* params,
                                   int argc, const char** argv,
                                   const char** invalidp);
    virtual bool parse_nexthop(Link* link, LinkParams* params);
    virtual CLConnection* new_connection(LinkParams* params);
    /// @}

    /**
     * Helper class (and thread) that listens on a registered
     * interface for new connections.
     */
    class Listener : public CLInfo, public oasys::TCPServerThread {
    public:
        Listener(TCPConvergenceLayer* cl);
        void accepted(int fd, in_addr_t addr, u_int16_t port);

        /// The TCPCL instance
        TCPConvergenceLayer* cl_;
    };

    /**
     * Helper class (and thread) that manages an established
     * connection with a peer daemon.
     *
     * Although the same class is used in both cases, a particular
     * Connection is either a receiver or a sender, as indicated by
     * the direction variable. Note that to deal with NAT, the side
     * which does the active connect is not necessarily the sender.
     */
    class Connection : public StreamConvergenceLayer::Connection {
    public:
        /**
         * Constructor for the active connect side of a connection.
         */
        Connection(TCPConvergenceLayer* cl, TCPLinkParams* params);

        /**
         * Constructor for the passive accept side of a connection.
         */
        Connection(TCPConvergenceLayer* cl, TCPLinkParams* params,
                   int fd, in_addr_t addr, u_int16_t port);

        /**
         * Destructor.
         */
        virtual ~Connection();

    protected:
        friend class TCPConvergenceLayer;

        /// @{ Virtual from CLConnection
        virtual void connect();
        virtual void accept();
        virtual void disconnect();
        virtual void initialize_pollfds();
        virtual void handle_poll_activity();
        /// @}

        /// @{ virtual from StreamConvergenceLayer::Connection
        void send_data();
        /// @}
        
        void recv_data();
        bool recv_contact_header(int timeout);
        bool send_bundle(Bundle* bundle);
        bool recv_bundle();
        bool handle_reply();
        int handle_ack();
        bool send_ack(u_int32_t bundle_id, size_t acked_len);
        bool send_keepalive();

        /**
         * Utility function to downcast the params_ pointer that's
         * stored in the CLConnection parent class.
         */
        TCPLinkParams* tcp_lparams()
        {
            TCPLinkParams* ret = dynamic_cast<TCPLinkParams*>(params_);
            ASSERT(ret != NULL);
            return ret;
        }
        
        oasys::TCPClient* sock_;	///< The socket
        struct pollfd*	  sock_pollfd_;	///< Poll structure for the socket
    };
};

} // namespace dtn

#endif /* _TCP_CONVERGENCE_LAYER_H_ */

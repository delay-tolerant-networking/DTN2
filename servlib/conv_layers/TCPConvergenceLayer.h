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

#include "IPConvergenceLayer.h"

namespace dtn {

class TCPConvergenceLayer : public IPConvergenceLayer {
public:
    /**
     * Constructor.
     */
    TCPConvergenceLayer();
    
    /**
     * The meat of the initialization happens here.
     */
    virtual void init();

    /**
     * Hook for shutdown.
     */
    virtual void fini();

    /**
     * Register a new interface.
     */
    bool add_interface(Interface* iface, int argc, const char* argv[]);

    /**
     * Remove an interface
     */
    bool del_interface(Interface* iface);

    /**
     * Open the connection to the given contact and prepare for
     * bundles to be transmitted.
     */
    bool open_contact(Contact* contact);
    
    /**
     * Close the connnection to the contact.
     */
    bool close_contact(Contact* contact);

    /**
     * Tunable parameters
     */
    static struct _defaults {
        int ack_blocksz_;
        int keepalive_interval_;
        u_int32_t idle_close_time_;
        int test_fragment_size_;
    } Defaults;

protected:
    /**
     * Helper class (and thread) that listens on a registered
     * interface for new connections.
     */
    class Listener : public InterfaceInfo, public oasys::TCPServerThread {
    public:
        Listener();
        void accepted(int fd, in_addr_t addr, u_int16_t port);
    };

    /**
     * Helper class (and thread) that manages an established
     * connection with a peer daemon.
     *
     * Although the same class is used in either case, a particular
     * Connection is either a receiver or a sender, as negotiated by
     * the convergence layer specific framing protocol.
     */
    class Connection : public ContactInfo, public oasys::Thread, public oasys::Logger {
    public:
        /**
         * Constructor for the active connection side of a connection.
         */
        Connection(Contact* contact,
                   in_addr_t remote_addr,
                   u_int16_t remote_port);
        
        /**
         * Constructor for the passive listener side of a connection.
         */
        Connection(int fd,
                   in_addr_t remote_addr,
                   u_int16_t remote_port);

        /**
         * Destructor.
         */
        ~Connection();
        
    protected:
        virtual void run();
        
        bool connect(in_addr_t remote_addr, u_int16_t remote_port);
        bool accept();

        /**
         * This is used to inform the rest of the system
         * that contact is broken
         */
        void break_contact();
        
        void send_loop();
        void recv_loop();

        bool send_bundle(Bundle* bundle, size_t* acked_len);
        int handle_ack(Bundle* bundle, size_t* acked_len);
        void note_data_rcvd();

        bool recv_bundle();
        bool send_ack(u_int32_t bundle_id, size_t acked_len);
        Contact* contact_;
        oasys::TCPClient* sock_;
        size_t ack_blocksz_;
        int keepalive_msec_;
        int idle_close_time_;
        struct timeval data_rcvd_;
    };
};

#ifndef MIN
# define MIN(x, y) ((x)<(y) ? (x) : (y))
#endif

} // namespace dtn

#endif /* _TCP_CONVERGENCE_LAYER_H_ */

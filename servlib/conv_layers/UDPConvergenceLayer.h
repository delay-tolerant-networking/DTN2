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
#ifndef _UDP_CONVERGENCE_LAYER_H_
#define _UDP_CONVERGENCE_LAYER_H_

#include <oasys/io/UDPClient.h>
#include <oasys/thread/Thread.h>

#include "IPConvergenceLayer.h"

namespace dtn {

class UDPConvergenceLayer : public IPConvergenceLayer {
public:
    /**
     * Maximum bundle size
     */
    static const u_int MAX_BUNDLE_LEN = 65507;

    /**
     * Constructor.
     */
    UDPConvergenceLayer();
        
    /**
     * Bring up a new interface.
     */
    bool interface_up(Interface* iface, int argc, const char* argv[]);

    /**
     * Bring down the interface.
     */
    bool interface_down(Interface* iface);
    
    /**
     * Dump out CL specific interface information.
     */
    void dump_interface(Interface* iface, oasys::StringBuffer* buf);

    /**
     * Create any CL-specific components of the Link.
     */
    bool init_link(Link* link, int argc, const char* argv[]);
    
    /**
     * Dump out CL specific link information.
     */
    void dump_link(Link* link, oasys::StringBuffer* buf);
    
    /**
     * Open the connection to a given contact and send/listen for 
     * bundles over this contact.
     */
    bool open_contact(Link* link);
    
    /**
     * Close the connnection to the contact.
     */
    bool close_contact(const ContactRef& contact);

    /**
     * Send the bundle out the link.
     */
    void send_bundle(const ContactRef& contact, Bundle* bundle);

    /**
     * Tunable parameter structure.
     *
     * Per-link and per-interface settings are configurable via
     * arguments to the 'link add' and 'interface add' commands.
     *
     * The parameters are stored in each Link's CLInfo slot, as well
     * as part of the Receiver helper class.
     */
    class Params : public CLInfo {
    public:
        in_addr_t local_addr_;		///< Local address to bind to
        u_int16_t local_port_;		///< Local port to bind to
        in_addr_t remote_addr_;		///< Peer address to connect to
        u_int16_t remote_port_;		///< Peer port to connect to
    };
    
    /**
     * Default parameters.
     */
    static Params defaults_;

protected:
    bool parse_params(Params* params, int argc, const char** argv,
                      const char** invalidp);
    /**
     * Helper class (and thread) that listens on a registered
     * interface for incoming data.
     */
    class Receiver : public CLInfo,
                     public oasys::UDPClient,
                     public oasys::Thread
    {
    public:
        /**
         * Constructor.
         */
        Receiver(UDPConvergenceLayer::Params* params);

        /**
         * Destructor.
         */
        virtual ~Receiver() {}
        
        /**
         * Loop forever, issuing blocking calls to IPSocket::recvfrom(),
         * then calling the process_data function when new data does
         * arrive
         * 
         * Note that unlike in the Thread base class, this run() method is
         * public in case we don't want to actually create a new thread
         * for this guy, but instead just want to run the main loop.
         */
        void run();

        UDPConvergenceLayer::Params params_;
        
    protected:
        /**
         * Handler to process an arrived packet.
         */
        void process_data(u_char* bp, size_t len);
    };

    /*
     * Helper class that wraps the sender-side per-contact state.
     */
    class Sender : public CLInfo, public oasys::UDPClient {
    public:
        /**
         * Destructor.
         */
        virtual ~Sender() {}
        
    private:
        friend class UDPConvergenceLayer;
        
        /**
         * Constructor.
         */
        Sender(const ContactRef& contact);
        
        /**
         * Send one bundle.
         */
        bool send_bundle(Bundle* bundle);

        /**
         * The contact that we're representing.
         */
        ContactRef contact_;

        /**
         * Temporary buffer for formatting bundles. Note that the
         * fixed-length buffer is big enough since UDP packets can't
         * be any bigger than that.
         */
        u_char buf_[UDPConvergenceLayer::MAX_BUNDLE_LEN];
    };   
};

} // namespace dtn

#endif /* _UDP_CONVERGENCE_LAYER_H_ */

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
     * Current version of the protocol.
     */
    static const u_int8_t UDPCL_VERSION = 0x02;

    /**
     * The basic UDP header structure.
     */
    struct UDPCLHeader {
        u_int32_t magic;		///< magic word (MAGIC: "dtn!")
        u_int8_t  version;		///< udpcl protocol version
        u_int8_t  flags;		///< connection flags and operation
        u_int16_t source_id;		///< socket identifier at sender
        u_int32_t bundle_id;		///< bundle identifier at sender
    } __attribute__((packed));

    /**
     * Values for flags / op
     */
    typedef enum {
        RELIABLITY_REQUESTED  = 0x10,	///< request sliding-window reliability
                                        ///< (not implemented)
        BUNDLE_DATA  	      = 0x01,	///< bundle data transmission
    } header_flags_t;

    /**
     * Constructor.
     */
    UDPConvergenceLayer();
        
    /**
     * Register a new interface.
     */
    bool add_interface(Interface* iface, int argc, const char* argv[]);

    /**
     * Remove an interface
     */
    bool del_interface(Interface* iface);
    
    /**
     * Open the connection to a given contact and send/listen for 
     * bundles over this contact.
     */
    bool open_contact(Contact* contact);
    
    /**
     * Close the connnection to the contact.
     */
    bool close_contact(Contact* contact);

    /**
     * Send bundles queued for the contact. We only expect there to be
     * one bundle queued at any time since this is called immediately
     * when the bundle is queued on the contact.
     */
    void send_bundles(Contact* contact);

    /**
     * Helper class (and thread) that listens on a registered
     * interface for incoming data.
     */
    class Receiver : public CLInfo, public oasys::UDPClient, public oasys::Thread {
    public:
        /**
         * Constructor.
         */
        Receiver();

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
        Sender(Contact* contact);
        
        /**
         * Send one bundle.
         */
        bool send_bundle(Bundle* bundle);

        /**
         * The contact that we're representing.
         */
        Contact* contact_;
    };   
};

} // namespace dtn

#endif /* _UDP_CONVERGENCE_LAYER_H_ */

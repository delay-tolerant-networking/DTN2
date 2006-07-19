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
#ifndef _ETH_CONVERGENCE_LAYER_H_
#define _ETH_CONVERGENCE_LAYER_H_

// Only works on Linux (for now)
#ifdef XXX_demmer_fixme__linux

#include <sys/types.h>
#include <netinet/in.h>
#include <linux/if.h>

#include <oasys/thread/Thread.h>
#include <oasys/thread/Timer.h>

#include "ConvergenceLayer.h"
#include "naming/EthernetScheme.h" // for eth_addr_t

/** 
 *   The EthConvergenceLayer provides access to any ethernet interfaces that
 *  support RAW sockets. It periodically sends beacons out on each interface
 *  to support neighbor discovery. (this may change later). 
 *
 *   To add an ethernet interface in your config file, use:
 *
 *   interface add eth string://eth0
 *
 *   Theoretically, any router type should work, but so far, the only one I've
 *   been using is the NeighborhoodRouter. To use this router, add this to dtn.conf
 *  
 *   route set type neighborhood
 *
 */
namespace dtn {

class EthConvergenceLayer : public ConvergenceLayer {

public:
    class BeaconTimer;
    /**
     * Current version of the protocol.
     */
    static const u_int8_t ETHCL_VERSION = 0x01;
    static const u_int16_t MAX_ETHER_PACKET = 1518;
    static const u_int16_t ETHERTYPE_DTN = 0xd710;

    static const u_int8_t ETHCL_BEACON = 0x01;
    static const u_int8_t ETHCL_BUNDLE = 0x02;

    static const u_int32_t ETHCL_BEACON_TIMEOUT_INTERVAL = 2500; // 2.5 seconds

    /**
     * The basic Eth header structure.
     */
    struct EthCLHeader {
        u_int8_t  version;		///< ethcl protocol version
        u_int8_t  type;                 ///< 
        u_int16_t _padding2;            ///< 
        u_int32_t bundle_id;		///< bundle identifier at sender
    } __attribute__((packed));

    class EthCLInfo : public CLInfo {
      public:
        EthCLInfo(char* if_name) {            
	    memset(if_name_,0,IFNAMSIZ);
            strcpy(if_name_,if_name);
	    timer=NULL;
        }

	~EthCLInfo() {
	  if(timer)
	    delete timer;
	}
        
        // Name of the device 
        char if_name_[IFNAMSIZ]; 

	BeaconTimer* timer;
    };

    /**
     * Constructor.
     */
    EthConvergenceLayer();
        
    /**
     * Bring up a new interface.
     */
    bool interface_up(Interface* iface, int argc, const char* argv[]);

    /**
     * Bring down the interface.
     */
    bool interface_down(Interface* iface);
    
    /**
     * Open the connection to a given contact and send/listen for 
     * bundles over this contact.
     */
    bool open_contact(const ContactRef& contact);
    
    /**
     * Close the connnection to the contact.
     */
    bool close_contact(const ContactRef& contact);

    /**
     * Send the bundle to the contact
     */
    void send_bundle(const ContactRef& contact, Bundle* bundle);
    
    /**
     * Helper class (and thread) that listens on a registered
     * interface for incoming data.
     */
    class Receiver : public CLInfo, public oasys::Logger, public oasys::Thread {
    public:
        /**
         * Constructor.
         */
        Receiver(const char* if_name);

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
        char if_name_[IFNAMSIZ];
    };



    /**
     * Helper class (and thread) that manages an "established"
     * connection with a peer daemon (virtual connection in the
     * case of Eth).
     *
     * Only the sender side of the connection needs to initiate
     * a connection. The receiver will just receive data. Therefore,
     * we don't need a passive side of a connection
     */
    class Sender : public CLInfo, public oasys::Logger {
    public:
        /**
         * Constructor for the active connection side of a connection.
         */
        Sender(char* if_name, const ContactRef& contact);

        /**
         * Destructor.
         */
        virtual ~Sender() {}
        
    protected:
        friend class EthConvergenceLayer;
        
        /**
         * Send one bundle.
         */
        bool send_bundle(Bundle* bundle);

        /// The contact that we're representing
        ContactRef contact_;
        
        /// Socket identifier
        int sock_;

        /// MAC address of the interface used for this contact
        eth_addr_t src_hw_addr_;
        eth_addr_t dst_hw_addr_; 

	/// The name of the interface the next_hop is behind
        char if_name_[IFNAMSIZ]; 
        
	char canary_[7];
    };   

    /** 
     *  helper class (and thread) that periodically sends beacon messages
     *  over the specified ethernet interface.
    */
    class Beacon : public oasys::Logger, public oasys::Thread {
    public:
      Beacon(const char* if_name);

      virtual ~Beacon() {};

    private:
      virtual void run();
      char if_name_[IFNAMSIZ];
    };

    class BeaconTimer : public oasys::Logger, public oasys::Timer, public CLInfo {
    public:
        char * next_hop_;

        BeaconTimer(char * next_hop);
        ~BeaconTimer();

        void timeout(struct timeval* now);

        Timer* copy();
    };    

};


} // namespace dtn

#endif // __linux

#endif /* _ETH_CONVERGENCE_LAYER_H_ */

/*
 * License Agreement
 * 
 * NOTICE
 * This software (or technical data) was produced for the U. S.
 * Government under contract W15P7T-05-C-F600, and is
 * subject to the Rights in Data-General Clause 52.227-14 (JUNE 1987)
 * 
 * Copyright (C) 2006. The MITRE Corporation (http://www.mitre.org/).
 * All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * The US Government will not be charged any license fee and/or
 * royalties related to this software.
 * 
 * * Neither name of The MITRE Corporation; nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _EXTERNAL_ROUTER_H_
#define _EXTERNAL_ROUTER_H_

#include <config.h>
#ifdef XERCES_C_ENABLED

#include "BundleRouter.h"
#include "RouteTable.h"

#include <reg/Registration.h>
#include <oasys/serialize/XercesXMLSerialize.h>

#include <oasys/io/UDPClient.h>

#define EXTERNAL_ROUTER_SERVICE_TAG "/ext.rtr/*"

namespace dtn {

/**
 * ExternalRouter provides a plug-in interface for third-party
 * routing protocol implementations.
 *
 * Events received from BundleDaemon are serialized into
 * XML messages and UDP multicasted to external bundle router processes.
 * XML actions received on the interface are validated, transformed
 * into events, and placed on the global event queue.
 */
class ExternalRouter : public BundleRouter {
public:
    /// UDP port for IPC with external routers
    static u_int16_t server_port;

    /// Seconds between hello messages
    static u_int16_t hello_interval;

    /// Validate incoming XML messages
    static bool server_validation;

    /// XML schema required for XML validation
    static std::string schema;

    /// Include meta info in xml necessary for client validation 
    static bool client_validation;

    /// The static routing table
    static RouteTable *route_table;

    ExternalRouter();
    virtual ~ExternalRouter();

    /**
     * Called after all global data structures are set up.
     */
    virtual void initialize();

    /**
     * Format the given StringBuffer with static routing info.
     * @param buf buffer to fill with the static routing table
     */
    virtual void get_routing_state(oasys::StringBuffer* buf);

    /**
     * Builds a static route XML report
     */
    void build_route_report(oasys::SerializeAction *a);

    /**
     * Serialize events and UDP multicast to external routers.
     * @param event BundleEvent to process
     */
    virtual void handle_event(BundleEvent *event);

    /**
     * External router clean shutdown
     */
    virtual void shutdown();

protected:
    class ModuleServer;
    class HelloTimer;
    class ERRegistration;

    /// UDP server thread
    ModuleServer *srv_;

    /// The route table
    RouteTable *route_table_;

    /// ExternalRouter registration with the bundle forwarder
    ERRegistration *reg_;

    /// Hello timer
    HelloTimer *hello_;
};

/**
 * Helper class (and thread) that manages communications
 * with external routers
 */
class ExternalRouter::ModuleServer : public oasys::Thread,
                                     public oasys::UDPClient {
public:
    ModuleServer();
    virtual ~ModuleServer();

    /**
     * The main thread loop
     */
    virtual void run();

    /**
     * Parse incoming actions and place them on the
     * global event queue
     * @param payload the incoming XML document payload
     */
    void process_action(const char *payload);

    /**
     * Instantiates a new BundleEvent object of the given type.
     * @param event_tag create a corresponding BundleEvent from this tag
     * @return a BundleEvent or 0 if the event_tag is not valid
     */
    BundleEvent * instantiate(const char *event_tag);

    /// Message queue for accepting BundleEvents from ExternalRouter
    oasys::MsgQueue< std::string * > *eventq;

    /// Xerces XML validating parser for incoming messages
    oasys::XercesXMLUnmarshal parser_;

    oasys::SpinLock *lock_;
};

/**
 * Helper class for ExternalRouter hello messages
 */
class ExternalRouter::HelloTimer : public oasys::Timer {
public:
    HelloTimer(ExternalRouter *router);
    ~HelloTimer();
        
    /**
     * Timer callback function
     */
    void timeout(const struct timeval &now);

    ExternalRouter *router_;
};

/**
 * Helper class which registers to receive
 * bundles from remote peers
 */
class ExternalRouter::ERRegistration : public Registration {
public:
    ERRegistration(ExternalRouter *router);

    /**
     * Registration delivery callback function
     */
    void deliver_bundle(Bundle *bundle);

    ExternalRouter *router_;
};

/**
 * Global shutdown callback function
 */
void external_rtr_shutdown(void *args);

} // namespace dtn

#endif // XERCES_C_ENABLED
#endif //_EXTERNAL_ROUTER_H_

/*
 *    Copyright 2006-2007 The MITRE Corporation
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
 *
 *    The US Government will not be charged any license fee and/or royalties
 *    related to this software. Neither name of The MITRE Corporation; nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

#ifndef _EXTERNAL_ROUTER_H_
#define _EXTERNAL_ROUTER_H_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include "router-custom.h"
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
     * External router clean shutdown
     */
    virtual void shutdown();

    /**
     * Format the given StringBuffer with static routing info.
     * @param buf buffer to fill with the static routing table
     */
    virtual void get_routing_state(oasys::StringBuffer* buf);

    /**
     * Serialize events and UDP multicast to external routers.
     * @param event BundleEvent to process
     */
    virtual void handle_event(BundleEvent *event);
    virtual void handle_bundle_received(BundleReceivedEvent *event);
    virtual void handle_bundle_transmitted(BundleTransmittedEvent* event);
    virtual void handle_bundle_delivered(BundleDeliveredEvent* event);
    virtual void handle_bundle_expired(BundleExpiredEvent* event);
    virtual void handle_bundle_cancelled(BundleSendCancelledEvent* event);
    virtual void handle_bundle_injected(BundleInjectedEvent* event);
    virtual void handle_contact_up(ContactUpEvent* event);
    virtual void handle_contact_down(ContactDownEvent* event);
    virtual void handle_link_created(LinkCreatedEvent *event);
    virtual void handle_link_deleted(LinkDeletedEvent *event);
    virtual void handle_link_available(LinkAvailableEvent *event);
    virtual void handle_link_unavailable(LinkUnavailableEvent *event);
    virtual void handle_link_attribute_changed(LinkAttributeChangedEvent *event);
    virtual void handle_contact_attribute_changed(ContactAttributeChangedEvent *event);
//    virtual void handle_link_busy(LinkBusyEvent *event);
    virtual void handle_new_eid_reachable(NewEIDReachableEvent* event);
    virtual void handle_registration_added(RegistrationAddedEvent* event);
    virtual void handle_registration_removed(RegistrationRemovedEvent* event);
    virtual void handle_registration_expired(RegistrationExpiredEvent* event);
    virtual void handle_route_add(RouteAddEvent* event);
    virtual void handle_route_del(RouteDelEvent* event);
    virtual void handle_custody_signal(CustodySignalEvent* event);
    virtual void handle_custody_timeout(CustodyTimeoutEvent* event);
    virtual void handle_link_report(LinkReportEvent *event);
    virtual void handle_link_attributes_report(LinkAttributesReportEvent *event);
    virtual void handle_contact_report(ContactReportEvent* event);
    virtual void handle_bundle_report(BundleReportEvent *event);
    virtual void handle_bundle_attributes_report(BundleAttributesReportEvent *event);
    virtual void handle_route_report(RouteReportEvent* event);

    virtual void send(rtrmessage::bpa &message);

protected:
    class ModuleServer;
    class HelloTimer;
    class ERRegistration;

    // XXX This function should really go in ContactEvent
    //     but ExternalRouter needs a less verbose version
    static const char *reason_to_str(int reason);

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

    /// Message queue for accepting BundleEvents from ExternalRouter
    oasys::MsgQueue< std::string * > *eventq;

    /// Xerces XML validating parser for incoming messages
    oasys::XercesXMLUnmarshal *parser_;

    oasys::SpinLock *lock_;

private:
    Link::link_type_t convert_link_type(rtrmessage::linkTypeType type);
    Bundle::priority_values_t convert_priority(rtrmessage::bundlePriorityType);

    ForwardingInfo::action_t 
    convert_fwd_action(rtrmessage::bundleForwardActionType);
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

#endif // XERCES_C_ENABLED && EXTERNAL_DP_ENABLED
#endif //_EXTERNAL_ROUTER_H_

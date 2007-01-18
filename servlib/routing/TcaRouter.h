/*
 *    Copyright 2005-2006 University of Waterloo
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


#ifndef _TCA_ROUTER_H_
#define _TCA_ROUTER_H_

#include "naming/EndpointID.h"
#include "TableBasedRouter.h"

#define SERVLIB 1
//#include "TcaTypes.h"
#include "TcaEndpointID.h"
#include "TcaControlBundle.h"


namespace dtn {


/**
 * This is the implementation of the TCA bundle routing algorithm.
 *
 * A TCARouter is a specialized TableBasedRouter where the route
 * table is manipulated in response to certain control bundles
 * (for example, a "register" bundle, or a "change-of-address" bundle).
 * Specialized routing logic is then applied in order to route late-bound
 * bundles addressed to a mobile node, to the mobile's current location
 * in the network.
 * 
 * The main interface point is the overridden handle_bundle_received
 * function which tests for the special TCA bundles (control bundles and
 * late-bound data bundles).
 *
 */
 
class TcaRouter : public TableBasedRouter {

public:

    enum Role { TCA_MOBILE, TCA_ROUTER, TCA_GATEWAY };

    // Internal bundle-forwarding rule.
    // This mostly has to do with how to treat the default route
    // (UDR = "Use Default Route").
    enum ForwardingRule {
        FWD_NEVER,              // do not forward, ever
        FWD_UDR_EXCLUSIVELY,    // forward (only) to the default route
        FWD_UDR_NEVER,          // fwd to matching, except default route
        FWD_UDR_IFNECESSARY,    // fwd to matching, using default route iff
                                // no other matches
        FWD_UDR_ALWAYS          // forward to matching, including default route
        };  

    TcaRouter(Role role);

protected:

    Role role_;

    TcaEndpointID admin_app_;   // eid of local admin application
    
    std::string get_role_str() const;

    // BundleEventHandler functions to handle events important to TCA.
    virtual void handle_bundle_received(BundleReceivedEvent* event);
    virtual void handle_bundle_transmitted(BundleTransmittedEvent* event);
    virtual void handle_contact_up(ContactUpEvent* event);
    virtual void handle_contact_down(ContactDownEvent* event);
    virtual void handle_link_available(LinkAvailableEvent* event);
    virtual void handle_link_unavailable(LinkUnavailableEvent* event);
    virtual void handle_shutdown_request(ShutdownRequest* event);

    // fwd function to broadcast a bundle to everybody in the route table
    virtual int fwd_to_all(Bundle* bundle);

    virtual int fwd_to_matching(Bundle* bundle, const LinkRef& next_hop);
    virtual int fwd_to_matching(Bundle* bundle) {
        LinkRef link("TcaRouter::fwd_to_matching: null");
        return fwd_to_matching(bundle, link);
    }
    
    // fwd function with special forwarding rules for default route
    // used for forwarding unbound tca bundles and some tca control bundles
    virtual int fwd_to_matching_r(Bundle* bundle, const LinkRef& next_hop,
                                  ForwardingRule fwd_rule);

    bool on_coa_transmitted(Bundle* b, const TcaControlBundle& cb);
    bool on_ask_transmitted(Bundle* b, const TcaControlBundle& cb);
    bool on_adv_transmitted(Bundle* b, const TcaControlBundle& cb);

    // special control bundle handlers
    bool handle_register(Bundle* b);
    bool handle_coa(Bundle* b);

    // handle bundle sent to anonymous address
    bool handle_anonymous_bundle(Bundle* b);

    bool handle_ask(Bundle* b, const TcaControlBundle& cb);

    // handle control bundles addressed to bundlelayer
    bool handle_bl_control_bundle(Bundle* b);

    bool handle_bl_ask(Bundle* b, const TcaControlBundle& cb);
    bool handle_get_routes(Bundle* b, const TcaControlBundle& cb);
    bool handle_add_route(Bundle* b, const TcaControlBundle& cb);
    bool handle_del_route(Bundle* b, const TcaControlBundle& cb);

    // handle regular late-bound tca data bundle
    bool handle_tca_unbound_bundle(Bundle* bundle);

    bool on_route_unbound_bundle(Bundle* bundle);
    bool on_gate_unbound_bundle(Bundle* bundle);

    // did the bundle originate at this node?
    bool is_local_source(Bundle* b);

    ForwardingRule get_forwarding_rule(Bundle* b);

    // create a link entry for the given address
    LinkRef create_link(const std::string& link_addr);

    // create a route entry for the given endpoint pattern, specified link
    RouteEntry* create_route(const std::string& pattern, const LinkRef& p_link);

    // create a route *and link* if necessary, for the given endpoint pattern,
    // given link address
    bool create_route(const std::string& pattern,
                      const std::string& link_addr);

    // Ultra-simplified helper function to inject a new bundle into
    // the works, using defaults for most fields.
    // Specify empty src for bundlelayer
    bool post_bundle(const EndpointID& src, const EndpointID& dest,
                     const std::string& payload);

    // Ultra-simplified helper function to post a wrapped bundle to the
    // admin app. This is in lieu of a WrappedBundle class.
    bool push_wrapped_bundle(const std::string& code,
                             const EndpointID& src,
                             const EndpointID& dest,
                             const std::string& bsp);

};

} // namespace dtn

#endif /* _TCA_ROUTER_H_ */

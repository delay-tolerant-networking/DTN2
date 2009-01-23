/*
 *    Copyright 2007 Intel Corporation
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

#ifndef _DTLSR_ROUTER_H_
#define _DTLSR_ROUTER_H_

#include "DTLSR.h"
#include "DTLSRConfig.h"

#include "BundleRouter.h"
#include "MultiGraph.h"
#include "TableBasedRouter.h"
#include "RouteEntry.h"
#include "reg/Registration.h"

namespace dtn {

/**
 * The DTLSRRouter uses link state announcements to build a
 * multigraph of routing edges. It uses the RouteTable inherited from
 * TableBasedRouter more as a FIB, installing an entry for the best
 * next-hop(s) for all known endpoints.
 */
class DTLSRRouter : public TableBasedRouter {
public:
    /// Constructor
    DTLSRRouter();

    /// @{ Virtual from BundleRouter
    void initialize();
    void get_routing_state(oasys::StringBuffer* buf);
    bool can_delete_bundle(const BundleRef& bundle);
    void delete_bundle(const BundleRef& bundle);
    /// @}
    
    /// @{ Event handlers 
    void handle_bundle_received(BundleReceivedEvent* e);
    void handle_bundle_expired(BundleExpiredEvent* e);
    void handle_contact_up(ContactUpEvent* e);
    void handle_contact_down(ContactDownEvent* e);
    void handle_link_created(LinkCreatedEvent* e);
    void handle_link_deleted(LinkDeletedEvent* e);
    void handle_registration_added(RegistrationAddedEvent* event);
    /// @}

protected:
    /// @{ Helper classes and typedefs
    class NodeInfo;
    class EdgeInfo;
    typedef MultiGraph<NodeInfo, EdgeInfo> RoutingGraph;
    friend class oasys::InlineFormatter<EdgeInfo>;
    class CostWeightFn;
    class DelayWeightFn;
    class EstimatedDelayWeightFn;
    typedef DTLSR::LinkParams LinkParams;
    typedef DTLSR::LinkState LinkState;
    typedef DTLSR::LinkStateVec LinkStateVec;
    typedef DTLSR::LSA LSA;
    /// @}
    
    //----------------------------------------------------------------------
    /// Class used for per-node state in the graph
    struct NodeInfo {
        NodeInfo()
            : last_lsa_seqno_(0),
              last_lsa_creation_ts_(0),
              last_eida_seqno_(0) {}
        
        u_int64_t last_lsa_seqno_;
        u_int64_t last_lsa_creation_ts_;
        
        u_int64_t last_eida_seqno_;

        // XXX/demmer put list of alternate endpoints for destination here
    };
    
    //----------------------------------------------------------------------
    /// Class used for per-edge state in the graph (the link)
    struct EdgeInfo {
        EdgeInfo(const std::string& id)
            : id_(id), params_(), is_registration_(false) {}
        EdgeInfo(const std::string& id, const LinkParams& params)
            : id_(id), params_(params), is_registration_(false) {}
        
        bool operator==(const EdgeInfo& other) const {
            return id_ == other.id_;
        }
        
        std::string id_;	      ///< link name
        LinkParams  params_;	      ///< link params
        oasys::Time last_update_;     ///< last time this edge was updated
        bool        is_registration_; ///< whether edge is local
    };

    //----------------------------------------------------------------------
    /// Class used for router-specific state in the routing table.
    struct RouteInfo : public RouteEntryInfo {
        RouteInfo(RoutingGraph::Edge* e) : edge_(e) {}
        RoutingGraph::Edge* edge_;
    };

    //----------------------------------------------------------------------
    /// Registration used to grab announcements
    class Reg : public Registration {
    public:
        Reg(DTLSRRouter* router, const EndpointIDPattern& eid);
        void deliver_bundle(Bundle* bundle);
    protected:
        DTLSRRouter* router_;
    };

    //----------------------------------------------------------------------
    class TransmitLSATimer : public oasys::Timer {
    public:
        TransmitLSATimer(DTLSRRouter* router)
            : router_(router), interval_(0) {}
        void timeout(const struct timeval& now);
        void set_interval(int interval) { interval_ = interval; }

    protected:
        DTLSRRouter* router_;
        int interval_;
    };

    /// @{ Helper functions
    const DTLSRConfig* config() { return DTLSRConfig::instance(); }
    void generate_link_state(LinkState* ls,
                             RoutingGraph::Edge* e,
                             const LinkRef& link);
    bool update_current_lsa(RoutingGraph::Node* node,
                            Bundle* bundle, u_int64_t seqno);
    void schedule_lsa();
    void send_lsa();
    void handle_lsa(Bundle* bundle, LSA* lsa);
    void handle_lsa_expired(Bundle* bundle);
    void drop_all_links(const EndpointID& source);
    static bool is_dynamic_route(RouteEntry* entry);
    
    void remove_edge(RoutingGraph::Edge* edge);
    void adjust_uptime(RoutingGraph::Edge* edge);

    bool time_to_age_routes();
    void invalidate_routes();
    void recompute_routes();
    /// @}

    //--------------------------------------------------------------------
    /// Service tag used for routing announcements
    const char* announce_tag_;
    
    /// Endpoint id used for routing announcements
    EndpointID announce_eid_;

    /// Routing Graph info
    RoutingGraph graph_;
    RoutingGraph::Node* local_node_;
    RoutingGraph::WeightFn* weight_fn_;

    /// Bundle lists used to hold onto the most recent LSA from all
    /// other nodes.
    /// 
    /// XXX/demmer this would be better done using a retention
    /// constraint :)
    BundleList current_lsas_;

    /// The registration to receive lsa and eida announcements
    Reg* reg_;

    /// Timer to periodically rebroadcast LSAs 
    TransmitLSATimer periodic_lsa_timer_;

    /// Timer used for a deferred LSA transmission, waiting for the
    /// minimum interval
    TransmitLSATimer delayed_lsa_timer_;

    /// Time of the last LSA transmission
    oasys::Time last_lsa_transmit_;

    /// Time of the last update of local graph
    oasys::Time last_update_;
};

} // namespace dtn

#endif /* _DTLSR_ROUTER_H_ */

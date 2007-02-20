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

#ifndef _SHORTEST_PATH_ROUTER_H_
#define _SHORTEST_PATH_ROUTER_H_

#include "BundleRouter.h"
#include "MultiGraph.h"
#include "DuplicateCache.h"
#include "naming/EndpointID.h"
#include "reg/Registration.h"

namespace dtn {

class ShortestPathRouter : public BundleRouter  {
public:
    typedef enum {
        LINK_INFO = 1,
    } msg_type_t;

    /// Link parameters that are sent over the network.
    struct LinkParams {
        LinkParams() : cost_(0), delay_(0), bw_(0) {}
        
        u_int32_t cost_;
        u_int32_t delay_;
        u_int32_t bw_;
    } __attribute__((packed)) ;

    /// Structure used for link state announcements
    struct LSA {
        EndpointID source_;
        EndpointID dest_;
        std::string id_;
        LinkParams params_;
    };

    /// A vector of LSAs
    typedef std::vector<LSA> LSAVector;

    /// Constructor
    ShortestPathRouter();

    /// @{ Virtual from BundleRouter
    void handle_event(BundleEvent* event);
    void initialize();
    void get_routing_state(oasys::StringBuffer* buf);
    /// @}
    
    /// @{ Event handlers
    void handle_bundle_received(BundleReceivedEvent* e);
    void handle_link_created(LinkCreatedEvent* e);
    void handle_link_deleted(LinkDeletedEvent* e);
    /// @}

    /// Class used for per-node state in the graph (none)
    struct NodeInfo {};

    /// Class used for per-edge state in the graph (the link)
    struct EdgeInfo {
        EdgeInfo(const std::string& id) : id_(id) {}
        EdgeInfo(const std::string& id, const LinkParams& params)
            : id_(id), params_(params) {}

        std::string id_;
        LinkParams params_;
    };

    /// Registration used to grab announcements
    class SPReg : public Registration {
    public:
        SPReg(ShortestPathRouter* router, const EndpointIDPattern& eid);
        void deliver_bundle(Bundle* bundle);
    protected:
        ShortestPathRouter* router_;
    };

    /// Endpoint id used for routing announcements
    EndpointID announce_eid_;

    /// Typedef for the routing graph
    typedef MultiGraph<NodeInfo, EdgeInfo> RoutingGraph;
    
protected:
    /**
     * Simple weight function that uses the edge cost parameter.
     */
    class CostWeightFn : public RoutingGraph::WeightFn {
    public:
        u_int32_t operator()(RoutingGraph::Edge* edge);
    };

    /// @{ Helper functions
    bool parse_lsa_bundle(Bundle* bundle, LSAVector* lsa_vec);
    void format_lsa_bundle(Bundle* bundle, LSAVector* lsa_vec);
    void try_to_forward_bundle(Bundle* b);
    void unicast_bundle(Bundle* b);
    void multicast_bundle(Bundle* b);
    void handle_lsa_bundle(Bundle* b);
    void generate_lsa(LSA* lsa, RoutingGraph::Edge* edge);
    void broadcast_announcement(RoutingGraph::Edge* edge);
    void dump_state(const LinkRef& link);
    /// @}

    RoutingGraph graph_;
    RoutingGraph::Node* local_node_;
    RoutingGraph::WeightFn* weight_fn_;
    BundleList forward_pending_;
    SPReg* reg_;
    DuplicateCache dupcache_;
};

} // namespace dtn

#endif /* _SHORTEST_PATH_ROUTER_H_ */

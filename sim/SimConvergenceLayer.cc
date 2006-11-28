/*
 *    Copyright 2004-2006 Intel Corporation
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


#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>

#include "SimConvergenceLayer.h"
#include "Node.h"
#include "Simulator.h"
#include "Topology.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleList.h"

namespace dtnsim {

/**
 * Simulator implementation of the CLInfo abstract class
 */
class SimCLInfo : public CLInfo {
public:
    SimCLInfo()
    {
        params_.deliver_partial_ = true;
        params_.reliable_ = true;
    }

    ~SimCLInfo() {};

    struct Params {
        /// if contact closes in the middle of a transmission, deliver
        /// the partially received bytes to the router.
        bool deliver_partial_;

        /// for bundles sent over the link, signal to the router
        /// whether or not they were delivered reliably by the
        /// convergence layer
        bool reliable_;
        
    } params_;

    Node* peer_node_;	///< The receiving node
};

SimConvergenceLayer* SimConvergenceLayer::instance_;

SimConvergenceLayer::SimConvergenceLayer()
    : ConvergenceLayer("SimConvergenceLayer", "sim")
{
}

bool
SimConvergenceLayer::init_link(Link* link, int argc, const char* argv[])
{
    oasys::OptParser p;

    SimCLInfo* info = new SimCLInfo();
    info->peer_node_ = Topology::find_node(link->nexthop());
    ASSERT(info->peer_node_);

    p.addopt(new oasys::BoolOpt("deliver_partial",
                                &info->params_.deliver_partial_));
    p.addopt(new oasys::BoolOpt("reliable", &info->params_.reliable_));
    
    const char* invalid;
    if (! p.parse(argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option %s", invalid);
        return false;
    }

    link->set_cl_info(info);

    return true;
}

bool
SimConvergenceLayer::open_contact(const ContactRef& contact)
{
    log_debug("opening contact for link [*%p]", contact.object());
    
    BundleDaemon::post(new ContactUpEvent(contact));
	
    return true;
}

void 
SimConvergenceLayer::send_bundle(const ContactRef& contact, Bundle* bundle)
{
    log_debug("send_bundles on contact %s", contact->link()->nexthop());

    SimCLInfo* info = (SimCLInfo*)contact->link()->cl_info();
    ASSERT(info);

    // XXX/demmer add Connectivity check to see if it's open and add
    // bw/latency restrictions. then move the following events to
    // there

    Node* src_node = Node::active_node();
    Node* dst_node = info->peer_node_;

    ASSERT(src_node != dst_node);

    size_t len;
    bool reliable = info->params_.reliable_;
	
    //len = bundle->payload_.length();
    len = BundleProtocol::formatted_length(bundle);
	
    BundleTransmittedEvent* tx_event =
        new BundleTransmittedEvent(bundle, contact, contact->link(),
                                   len, reliable ? len : 0);

    Simulator::post(new SimRouterEvent(Simulator::time(),
                                       src_node, tx_event));

    BundleReceivedEvent* rcv_event =
        new BundleReceivedEvent(bundle, EVENTSRC_PEER, len);
    
    Simulator::post(new SimRouterEvent(Simulator::time(),
                                       dst_node, rcv_event));
}


} // namespace dtnsim

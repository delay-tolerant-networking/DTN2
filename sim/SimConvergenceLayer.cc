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
        params_.reliable_        = true;
        params_.delay_           = 0.001; // 1ms
        params_.bps_             = 1000000; // 1Mbps
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

        /// configurable delay
        double delay_;

        /// configurable bandwidth
        u_int bps_;
        
    } params_;

    Node* peer_node_;	///< The receiving node
};

SimConvergenceLayer* SimConvergenceLayer::instance_;

SimConvergenceLayer::SimConvergenceLayer()
    : ConvergenceLayer("SimConvergenceLayer", "sim")
{
}

bool
SimConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);

    oasys::OptParser p;

    SimCLInfo* info = new SimCLInfo();
    info->peer_node_ = Topology::find_node(link->nexthop());
    ASSERT(info->peer_node_);

    p.addopt(new oasys::BoolOpt("deliver_partial",
                                &info->params_.deliver_partial_));
    p.addopt(new oasys::BoolOpt("reliable", &info->params_.reliable_));
    p.addopt(new oasys::DoubleOpt("delay", &info->params_.delay_));
    p.addopt(new oasys::UIntOpt("bps", &info->params_.bps_));
    
    const char* invalid;
    if (! p.parse(argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option %s", invalid);
        return false;
    }

    link->set_cl_info(info);

    return true;
}

void
SimConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("SimConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(NULL);
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
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("send_bundle *%p on link *%p", bundle, link.object());

    SimCLInfo* info = (SimCLInfo*)link->cl_info();
    ASSERT(info);

    // XXX/demmer add Connectivity check to see if it's open and add
    // bw/delay restrictions. then move the following events to
    // there

    Node* src_node = Node::active_node();
    Node* dst_node = info->peer_node_;

    ASSERT(src_node != dst_node);

    bool reliable = info->params_.reliable_;

    BlockInfoVec* blocks = bundle->xmit_blocks_.find_blocks(link);
    ASSERT(blocks != NULL);

    // since we don't really have any payload to send, we find the
    // payload block and overwrite the data_length to be zero, then
    // adjust the payload_ on the new bundle
    if (bundle->payload_.location() == BundlePayload::NODATA) {
        BlockInfo* payload = const_cast<BlockInfo*>(
            blocks->find_block(BundleProtocol::PAYLOAD_BLOCK));
        ASSERT(payload != NULL);
        payload->set_data_length(0);
    }
    
    bool complete = false;
    size_t len = BundleProtocol::produce(bundle, blocks,
                                         buf_, 0, sizeof(buf_),
                                         &complete);
    ASSERTF(complete, "BundleProtocol non-payload blocks must fit in "
            "65 K buffer size");

    size_t total_len = len + bundle->payload_.length();

    complete = false;
    Bundle* new_bundle = new Bundle(bundle->payload_.location());
    int cc = BundleProtocol::consume(new_bundle, buf_, len, &complete);
    ASSERT(cc == (int)len);
    ASSERT(complete);

    if (bundle->payload_.location() == BundlePayload::NODATA) {
        new_bundle->payload_.set_length(bundle->payload_.length());
    }
            
    BundleTransmittedEvent* tx_event =
        new BundleTransmittedEvent(bundle, contact, link,
                                   total_len, reliable ? total_len : 0);
    src_node->post_event(tx_event);

    BundleReceivedEvent* rcv_event =
        new BundleReceivedEvent(new_bundle, EVENTSRC_PEER, total_len);

    double arrival_time = Simulator::time() + info->params_.delay_;
    Simulator::post(new SimBundleEvent(arrival_time, dst_node, rcv_event));
}


} // namespace dtnsim

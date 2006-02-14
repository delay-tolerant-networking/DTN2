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
    : ConvergenceLayer("sim")
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
SimConvergenceLayer::open_contact(Contact* contact)
{
    log_debug("open_contact %s", contact->nexthop());
    BundleDaemon::post(new ContactUpEvent(contact));
    return true;
}

void 
SimConvergenceLayer::send_bundle(Contact* contact, Bundle* bundle)
{
    log_debug("send_bundles on contact %s", contact->nexthop());

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

    len = bundle->payload_.length();
       
    BundleTransmittedEvent* tx_event =
        new BundleTransmittedEvent(bundle, contact, len, reliable ? len : 0);

    Simulator::post(new SimRouterEvent(Simulator::time(),
                                       src_node, tx_event));

    BundleReceivedEvent* rcv_event =
        new BundleReceivedEvent(bundle, EVENTSRC_PEER, len);
    
    Simulator::post(new SimRouterEvent(Simulator::time(),
                                       dst_node, rcv_event));
}


} // namespace dtnsim

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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <queue>

#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/TokenBucket.h>

#include "SimConvergenceLayer.h"
#include "Connectivity.h"
#include "Node.h"
#include "Simulator.h"
#include "Topology.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleList.h"
#include "contacts/ContactManager.h"

namespace dtnsim {

class InFlightBundle;

//----------------------------------------------------------------------
class SimLink : public CLInfo,
                public oasys::Logger,
                public oasys::Timer {
public:
    struct Params;
    
    SimLink(const LinkRef& link,
            const SimLink::Params& params)
        : Logger("/dtn/cl/sim/%s", link->name()),
          link_(link.object(), "SimLink"),
          params_(params),
          tb_(((std::string)logpath_ + "/tb").c_str(),
              params_.capacity_,
              0xffffffff /* unlimited rate */),
          reopen_timer_(link)
    {
    }

    ~SimLink() {};

    void send_bundle(Bundle* bundle, size_t total_len, const ConnState& cs);

    void timeout(const timeval& now);
    void reschedule_arrival();

    /// The dtn Link
    LinkRef link_;
    
    struct Params {
        /// if contact closes in the middle of a transmission, deliver
        /// the partially received bytes to the router.
        bool deliver_partial_;

        /// for bundles sent over the link, signal to the router
        /// whether or not they were delivered reliably by the
        /// convergence layer
        bool reliable_;

        /// capacity of the link
        u_int capacity_;

        /// automatically infer the remote eid when the link connects
        bool set_remote_eid_;
        
        /// set the previous hop when bundles arrive
        bool set_prevhop_;
        
    } params_;

    /// The receiving node
    Node* peer_node_;	

    /// Helper class to track in flight bundles
    struct InFlightBundle {
        InFlightBundle(Bundle*            bundle,
                       size_t             total_len,
                       const oasys::Time& arrival_time)
            : bundle_(bundle, "SimCL::InFlightBundle"),
              total_len_(total_len),
              arrival_time_(arrival_time) {}

        BundleRef   bundle_;
        size_t      total_len_;
        oasys::Time arrival_time_;
    };

    /// The List of in flight bundles
    std::queue<InFlightBundle*> inflight_;

    /// Token bucket to track the link rate
    oasys::TokenBucket tb_;

    /// Helper class to unblock the link
    class ReopenTimer : public oasys::Timer {
    public:
        ReopenTimer(const LinkRef& link)
            : link_(link.object(), "SimLink::ReopenTimer") {}
        
        void timeout(const timeval& tv);
        LinkRef link_;
    };

    /// The reopening timer
    ReopenTimer reopen_timer_;
};

//----------------------------------------------------------------------
void
SimLink::timeout(const timeval& tv)
{
    oasys::Time now(tv.tv_sec, tv.tv_usec);
    
    ASSERT(!inflight_.empty());

    // deliver any bundles that have arrived
    while (!inflight_.empty()) {
        InFlightBundle* next = inflight_.front();
        if (next->arrival_time_ <= now) {
            const BundleRef& bundle = next->bundle_;
            
            inflight_.pop();
            BundleReceivedEvent* rcv_event =
                new BundleReceivedEvent(bundle.object(),
                                        EVENTSRC_PEER,
                                        next->total_len_,
                                        NULL,
                                        params_.set_prevhop_ ?
                                          Node::active_node()->local_eid() :
                                          EndpointID::NULL_EID());
            peer_node_->post_event(rcv_event);

            delete next;
            
        } else {
            break;
        }
    }

    reschedule_arrival();
}

//----------------------------------------------------------------------
void
SimLink::send_bundle(Bundle* bundle, size_t total_len, const ConnState& cs)
{
    oasys::Time arrival_time(Simulator::time() + cs.latency_);
    inflight_.push(new InFlightBundle(bundle, total_len, arrival_time));
    reschedule_arrival();

    bool full = !tb_.drain(total_len);
    
    log_debug("send_bundle %d: (total len %zu), link %s",
              bundle->bundleid_, total_len,
              full ? "capacity full" : "still has capacity");
    
    if (full)
    {
        if (link_->state() == Link::OPEN) {
            link_->set_state(Link::BUSY);
            BundleDaemon::post_at_head(new LinkBusyEvent(link_));
        }
        
        if (! reopen_timer_.pending()) {
            // wait until we re-reach the bucket's level plus some slop
            // for the next bundle
            u_int32_t next_send = tb_.time_to_level(1024);
            log_debug("scheduling reopen timer in %u ms", next_send);
            reopen_timer_.schedule_in(next_send);
        }
    }
}

//----------------------------------------------------------------------
void
SimLink::reschedule_arrival()
{
    if (pending_)
        return;

    if (inflight_.empty()) {
        return;
    }

    InFlightBundle* next = inflight_.front();

    struct timeval tv;
    tv.tv_sec  = next->arrival_time_.sec_;
    tv.tv_usec = next->arrival_time_.usec_;
    schedule_at(&tv);
}

//----------------------------------------------------------------------
void
SimLink::ReopenTimer::timeout(const timeval& /*tv*/)
{
    if (link_->state() == Link::BUSY) {
        BundleDaemon::post_at_head(
            new LinkStateChangeRequest(link_,
                                       Link::AVAILABLE,
                                       ContactEvent::UNBLOCKED));
    } else {
        log_warn_p("/dtn/sim/cl",
                   "reopen timer fired in bad state for link *%p",
                   link_.object());
    }
}
 
//----------------------------------------------------------------------
SimConvergenceLayer* SimConvergenceLayer::instance_;

SimConvergenceLayer::SimConvergenceLayer()
    : ConvergenceLayer("SimConvergenceLayer", "sim")
{
}

//----------------------------------------------------------------------
bool
SimConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);

    oasys::OptParser p;
    SimLink::Params params;

    params.deliver_partial_ = true;
    params.reliable_        = true;
    params.capacity_        = 64 * 1000; // 64KB
    params.set_remote_eid_  = true;
    params.set_prevhop_     = true;

    p.addopt(new oasys::BoolOpt("deliver_partial", &params.deliver_partial_));
    p.addopt(new oasys::BoolOpt("reliable", &params.reliable_));
    p.addopt(new oasys::UIntOpt("capacity", &params.capacity_));
    p.addopt(new oasys::BoolOpt("set_remote_eid", &params.set_remote_eid_));
    p.addopt(new oasys::BoolOpt("set_prevhop", &params.set_prevhop_));

    const char* invalid;
    if (! p.parse(argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option %s", invalid);
        return false;
    }

    SimLink* sl = new SimLink(link, params);
    sl->peer_node_ = Topology::find_node(link->nexthop());

    ASSERT(sl->peer_node_);
    link->set_cl_info(sl);

    return true;
}

//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
bool
SimConvergenceLayer::open_contact(const ContactRef& contact)
{
    log_debug("opening contact for link [*%p]", contact.object());


    SimLink* sl = (SimLink*)contact->link()->cl_info();
    ASSERT(sl);
    
    const ConnState* cs = Connectivity::instance()->
                          lookup(Node::active_node(), sl->peer_node_);
    ASSERTF(cs, "can't find connectivity state from %s -> %s",
            Node::active_node()->name(), sl->peer_node_->name());

    if (cs->open_) {
        log_debug("opening contact");
        if (sl->params_.set_remote_eid_) {
            contact->link()->set_remote_eid(sl->peer_node_->local_eid());
        }
        BundleDaemon::post(new ContactUpEvent(contact));
    } else {
        log_debug("connectivity is down when trying to open contact");
        BundleDaemon::post(
            new LinkStateChangeRequest(contact->link(),
                                       Link::CLOSED,
                                       ContactEvent::BROKEN));
    }
	
    return true;
}

//----------------------------------------------------------------------
void 
SimConvergenceLayer::send_bundle(const ContactRef& contact, Bundle* bundle)
{
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("send_bundle *%p on link *%p", bundle, link.object());

    SimLink* sl = (SimLink*)link->cl_info();
    ASSERT(sl);

    // XXX/demmer add Connectivity check to see if it's open and add
    // bw/delay restrictions. then move the following events to
    // there

    Node* src_node = Node::active_node();
    Node* dst_node = sl->peer_node_;

    ASSERT(src_node != dst_node);

    const ConnState* cs = Connectivity::instance()->lookup(src_node, dst_node);
    ASSERT(cs);
    
    bool reliable = sl->params_.reliable_;

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

    if (link->state() != Link::OPEN) {
        log_warn("send_bundle in link state %s",
                 link->state_to_str(link->state()));
    }

    sl->send_bundle(new_bundle, total_len, *cs);
}

//----------------------------------------------------------------------
void
SimConvergenceLayer::update_connectivity(Node* n1, Node* n2, const ConnState& cs)
{
    ASSERT(n1 != NULL);
    ASSERT(n2 != NULL);

    n1->set_active();
    
    ContactManager* cm = n1->contactmgr();;

    oasys::ScopeLock l(cm->lock(), "SimConvergenceLayer::update_connectivity");
    const LinkSet* links = cm->links();
    
    for (LinkSet::iterator iter = links->begin();
         iter != links->end();
         ++iter)
    {
        LinkRef link = *iter;
        SimLink* sl = (SimLink*)link->cl_info();
        ASSERT(sl);

        // update the token bucket
        sl->tb_.set_rate(cs.bw_ / 8);
        
        if (sl->peer_node_ != n2)
            continue;
        
        log_debug("update_connectivity: checking node %s link %s",
                  n1->name(), link->name());
        
        if (cs.open_ == false && link->state() == Link::OPEN) {
            log_debug("update_connectivity: closing link %s", link->name());
            n1->post_event(
                new LinkStateChangeRequest(link, Link::CLOSED,
                                           ContactEvent::BROKEN));
        }
    }
}


} // namespace dtnsim

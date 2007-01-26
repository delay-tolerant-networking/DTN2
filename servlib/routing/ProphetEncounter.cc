/*
 *    Copyright 2006 Baylor University
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

#include <netinet/in.h>
#include "bundling/Bundle.h"
#include "bundling/BundleRef.h"
#include "bundling/BundleList.h"
#include "bundling/BundleDaemon.h"
#include "ProphetEncounter.h"
#include "ProphetController.h"
#include <oasys/thread/Lock.h>
#include <oasys/util/Random.h>
#include <oasys/util/ScratchBuffer.h>

namespace dtn {

ProphetEncounter::ProphetEncounter(const LinkRef& nexthop,
                                   ProphetOracle* oracle)
    : oasys::Thread("ProphetEncounter",oasys::Thread::DELETE_ON_EXIT),
      oasys::Logger("ProphetEncounter","/dtn/route"),
      oracle_(oracle),
      remote_instance_(0),
      local_instance_(Prophet::UniqueID::instance()->instance_id()),
      tid_(0),
      timeout_(0),
      next_hop_(nexthop.object(), "ProphetEncounter"),
      synsender_(false),initiator_(false),
      synsent_(false),synrcvd_(false),
      estab_(false),
      neighbor_gone_(false),
      state_(WAIT_NB),
      cmdqueue_("/dtn/route"),
      to_be_fwd_("ProphetEncounter to be forwarded"),
      outbound_tlv_(NULL),
      state_lock_(new oasys::SpinLock()),
      otlv_lock_(new oasys::SpinLock())
{
    // validate inputs and assumptions
    ASSERT(oracle != NULL);
    ASSERT(local_instance_ != 0);
    ASSERT(nexthop != NULL);
    ASSERT(nexthop->local() != "");
    ASSERT(!nexthop->remote_eid().equals(EndpointID::NULL_EID()));

    logpath_appendf("/encounter-%d",local_instance_);

    // validate ProphetOracle contract
    ASSERT(oracle_->params() != NULL);
    ASSERT(oracle_->bundles() != NULL);
    ASSERT(oracle_->nodes() != NULL);
    ASSERT(oracle_->actions() != NULL);
    ASSERT(oracle_->acks() != NULL);
    ASSERT(oracle_->stats() != NULL);
    
    // set poll timeout
    // convert from units of 100ms to ms
    timeout_ = oracle->params()->hello_interval_ * 100;

    // zero out timers
    data_sent_.get_time();
    data_rcvd_.get_time();
    
    // initialize lists
    offers_.set_type(BundleOffer::OFFER);
    requests_.set_type(BundleOffer::RESPONSE);

    ack_count_ = 0;
}

ProphetEncounter::~ProphetEncounter() {
    // clean up the memory we're responsible for
    while(cmdqueue_.size() != 0)
    {
        ProphetEncounter::PEMsg pm;
        ASSERT(cmdqueue_.try_pop(&pm));
        if(pm.tlv_ != NULL)
            delete pm.tlv_;
    }
    oasys::ScopeLockIf l(otlv_lock_,"destructor",outbound_tlv_ != NULL);
    if (outbound_tlv_ != NULL) 
    {
        delete outbound_tlv_;
        outbound_tlv_ = NULL;
    }
    l.unlock();
    delete state_lock_;
    delete otlv_lock_;
}

bool
ProphetEncounter::operator< (const ProphetEncounter& p) const
{
    return local_instance_ < p.local_instance_;
}

bool
ProphetEncounter::operator< (u_int16_t inst) const
{
    return local_instance_ < inst;
}

ProphetEncounter::prophet_state_t
ProphetEncounter::get_state(const char *where)
{
    oasys::ScopeLock l(state_lock_, where);
    log_debug("get_state(%s) == %s",where,state_to_str(state_));
    return state_;
}

void
ProphetEncounter::set_state(prophet_state_t new_state)
{
    oasys::ScopeLock l(state_lock_,"ProphetEncounter::set_state");
    log_debug("set_state from %s to %s",
               state_to_str(state_),
               state_to_str(new_state));

    prophet_state_t oldstate = state_;
    state_ = new_state; 

    switch(new_state) {
    case WAIT_NB:
        // always legal
        synsent_ = false;
        synrcvd_ = false;
        estab_ = false;
        ack_count_ = 0;
        timeout_ = oracle_->params()->hello_interval_ * 100;
        break;
    case SYNSENT:
        ASSERT(oldstate == WAIT_NB ||
               oldstate == SYNSENT);
        synsent_ = true;
        break;
    case SYNRCVD:
        ASSERT(oldstate == WAIT_NB ||
               oldstate == SYNSENT ||
               oldstate == SYNRCVD);
        synrcvd_ = true;
        break;
    case ESTAB:
        ASSERT(oldstate == SYNRCVD ||
               oldstate == ESTAB ||
               oldstate == SYNSENT);
        estab_ = true;
        if (initiator_ == true)
        {
            set_state(CREATE_DR);
        }
        else
        {
            set_state(WAIT_DICT);
        }
        break;
    case WAIT_DICT:
        ASSERT(oldstate == ESTAB ||
               oldstate == WAIT_DICT ||
               oldstate == WAIT_RIB ||
               oldstate == OFFER ||
               oldstate == WAIT_INFO);
        break;
    case WAIT_RIB:
        ASSERT(oldstate == WAIT_DICT ||
               oldstate == WAIT_RIB);
        break;
    case OFFER:
        ASSERT(oldstate == OFFER ||
               oldstate == WAIT_RIB);
        if (oldstate == WAIT_RIB)
        {
            send_bundle_offer();
        }
        break;
    case CREATE_DR:
        ASSERT(oldstate == ESTAB ||
               oldstate == WAIT_INFO ||
               oldstate == WAIT_DICT ||
               oldstate == SEND_DR ||
               oldstate == REQUEST);
        break;
    case SEND_DR:
        ASSERT(oldstate == CREATE_DR ||
               oldstate == SEND_DR);
        break;
    case REQUEST:
        ASSERT(oldstate == REQUEST ||
               oldstate == SEND_DR);
        break;
    case WAIT_INFO:
        ASSERT(oldstate == REQUEST ||
               oldstate == OFFER);
        // On the first phase of INFO_EXCHANGE,
        // synsender_ is initiator and !synsender_ is
        // listener.  During this phase, immediately
        // switch roles and continue.
        if ((synsender_ && initiator_) ||
            (!synsender_ && !initiator_))
        {
            switch_info_role();
        }
        break;
    default:
        PANIC("Unknown Prophet state: %d", (int)new_state);
        break;
    }
}

void
ProphetEncounter::reset_link()
{
    log_debug("reset_link");
    neighbor_gone();
}

void
ProphetEncounter::handle_bundle_received(Bundle* bundle)
{
    BundleRef b("handle_bundle_received");
    b = bundle;
    log_debug("handle_bundle_received *%p",bundle);
    prophet_state_t state = get_state("handle_bundle_received");

    // Is this bundle destined for the attached peer?
    EndpointIDPattern route = Prophet::eid_to_route(next_hop_->remote_eid());
    if (route.match(b->dest_))
    {
        // short-circuit the protocol and forward to attached peer
        if (should_fwd(b.object(),false))
        {
            fwd_to_nexthop(b.object());
        }
    }

    // Check status of outstanding requests
    if (state == REQUEST)
    {
        u_int32_t cts = b->creation_ts_.seconds_;
        u_int32_t seq = b->creation_ts_.seqno_;
        u_int16_t sid = ribd_.find(Prophet::eid_to_routeid(b->dest_));
        requests_.remove_bundle(cts,seq,sid);

        // no requests remaining means the end of bundle exchange
        // so signal as such with 0-sized bundle request
        if (requests_.size() == 0)
        {
            enqueue_bundle_tlv(requests_);
            send_prophet_tlv();
            set_state(WAIT_INFO);
        }
    }
}

void
ProphetEncounter::receive_tlv(ProphetTLV* pt)
{
    log_debug("receive_tlv");
    // alert thread to new bundle
    cmdqueue_.push_back(PEMsg(PEMSG_PROPHET_TLV_RECEIVED,pt));
}

void
ProphetEncounter::neighbor_gone()
{
    log_debug("neighbor_gone");
    // alert thread to status change
    neighbor_gone_ = true;
    cmdqueue_.push_back(PEMsg(PEMSG_NEIGHBOR_GONE,NULL));
}

bool
ProphetEncounter::should_fwd(Bundle* b,bool hard_fail)
{
    BundleRef bundle("should_fwd");
    ForwardingInfo info;

    bundle = b;
    bool found = bundle->fwdlog_.get_latest_entry(next_hop_,&info);

    if (found) {
        ASSERT(info.state_ != ForwardingInfo::NONE);
    } else {
        ASSERT(info.state_ == ForwardingInfo::NONE);
    }

    if (info.state_ == ForwardingInfo::TRANSMITTED ||
        info.state_ == ForwardingInfo::IN_FLIGHT)
    {
        log_debug("should_fwd bundle %d: "
                  "skip %s due to forwarding log entry %s",
                  bundle->bundleid_, next_hop_->name(),
                  ForwardingInfo::state_to_str(
                      static_cast<ForwardingInfo::state_t>(info.state_)));
        if (hard_fail)
        {
            PANIC("neighbor requested bundle that has already been sent");
        }
        else
        {
            log_err("neighbor requested bundle that has already been sent");
        }
        return false;
    }

    if (info.state_ == ForwardingInfo::TRANSMIT_FAILED) {
        log_debug("should_fwd bundle %d: "
                  "match %s: forwarding log entry %s TRANSMIT_FAILED %d",
                  bundle->bundleid_, next_hop_->name(),
                  ForwardingInfo::state_to_str(
                      static_cast<ForwardingInfo::state_t>(info.state_)),
                  bundle->bundleid_); 
    } else {
        log_debug("should_fwd bundle %d: "
                  "match %s: forwarding log entry %s",
                  bundle->bundleid_, next_hop_->name(),
                  ForwardingInfo::state_to_str(
                      static_cast<ForwardingInfo::state_t>(info.state_)));
    }

    return true;
}

void
ProphetEncounter::fwd_to_nexthop(Bundle* b,bool add_front)
{
    BundleRef bundle("fwd_to_nexthop");
    bundle = b;
    log_debug("fwd_to_nexthop *%p at %s",b,add_front?"front":"back");

    // ProphetEncounter only exists if link is open
    ASSERT(next_hop_->isopen());

    if(bundle!=NULL)
    {
        if(add_front)
            //XXX/wilson this is naive, if more than one
            // gets enqueued we're busted
            
            // give priority to Prophet control messages
            to_be_fwd_.push_front(b);
        else
            to_be_fwd_.push_back(b);
    }

    // fill the pipe with however many bundles are pending
    while (next_hop_->isbusy() == false &&
           to_be_fwd_.size() > 0)
    {
        BundleRef ref("ProphetEncounter fwd_to_nexthop");
        ref = to_be_fwd_.pop_front();

        if (ref.object() == NULL)
        {
            log_err("Unexpected NULL pointer in to_be_fwd_ list");
            continue;
        }

        Bundle* b = ref.object();
        log_debug("sending *%p to *%p", b, next_hop_.object());

        bool ok = oracle_->actions()->send_bundle(b,next_hop_,
                                                  ForwardingInfo::COPY_ACTION,
                                                  CustodyTimerSpec());
        ASSERTF(ok == true,"failed to send bundle");

        // update Prophet stats on this bundle
        oracle_->stats()->update_stats(b,remote_nodes_.p_value(b));
    } 

    oasys::ScopeLock l(to_be_fwd_.lock(),"fwd_to_nexthop");
    for(BundleList::iterator i = to_be_fwd_.begin();
            i != to_be_fwd_.end(); i++)
    {
        log_debug("can't forward *%p to *%p because link is busy",
                   *i, next_hop_.object());
    }
}

void 
ProphetEncounter::handle_prophet_tlv(ProphetTLV* pt)
{
    ASSERT(pt != NULL);

    data_rcvd_.get_time();

    oasys::StringBuffer buf;
    pt->dump(&buf);
    log_debug("handle_prophet_tlv (tid %u,%s,%zu entries)\n"
              "Inbound TLV\n",
              pt->transaction_id(),
              Prophet::result_to_str(pt->result()),
              pt->num_tlv());
    log_debug("%s",buf.c_str());

    tid_ = pt->transaction_id();

    BaseTLV* tlv = NULL;
    bool ok = true;

    while (neighbor_gone_ == false &&
           (tlv = pt->get_tlv()) != NULL &&
           ok != false)
    {
        if (estab_ == false && tlv->typecode() != Prophet::HELLO_TLV)
        {
            delete tlv;
            handle_bad_protocol(pt->transaction_id());
            ok = false;
            break;
        }
    
        switch(tlv->typecode()) {
            case Prophet::HELLO_TLV:
                ok = handle_hello_tlv((HelloTLV*)tlv,pt);
                break;
            case Prophet::RIBD_TLV:
                ok = handle_ribd_tlv((RIBDTLV*)tlv,pt);
                break;
            case Prophet::RIB_TLV:
                ok = handle_rib_tlv((RIBTLV*)tlv,pt);
                break;
            case Prophet::BUNDLE_TLV:
                ok = handle_bundle_tlv((BundleTLV*)tlv,pt);
                break;
            case Prophet::UNKNOWN_TLV:
            case Prophet::ERROR_TLV:
            default:
                PANIC("Unexpected TLV type received by ProphetEncounter: %d",
                      tlv->typecode());
        }
        delete tlv;
    }
    if (ok != true && pt->list().size() != 0)
    {
        log_debug("killing off %zu unread TLVs",pt->list().size());
        // free up memory
        while ((tlv = pt->get_tlv()) != NULL)
            delete tlv;
    }
}

bool
ProphetEncounter::handle_hello_tlv(HelloTLV* ht,ProphetTLV* pt)
{
    ASSERT(ht != NULL);
    ASSERT(ht->typecode() == Prophet::HELLO_TLV);
    log_debug("handle_hello_tlv %s",Prophet::hf_to_str(ht->hf()));

    // Establish truth table based on section 5.2
    bool hello_a = (remote_instance_ == pt->sender_instance());
    bool hello_b = hello_a &&
                   (remote_addr_ == next_hop_->nexthop());
    bool hello_c = (local_instance_ == pt->receiver_instance());

    prophet_state_t state = get_state("handle_hello_tlv");
    log_info("received HF %s in state %s",Prophet::hf_to_str(ht->hf()),
             state_to_str(state));

    // negotiate to minimum timer (in 100ms units)
    u_int timeout = std::min(
                    (u_int)oracle_->params()->hello_interval_,
                    (u_int)ht->timer(),
                    std::less<u_int>());
    
    if ((timeout * 100) != timeout_) 
    {
        // timeout_ is in ms 
        timeout_ = timeout * 100;
    }

    if (ht->hf() == Prophet::RSTACK)
    {
        if (hello_a && hello_c && synsent_ == false)
        {
            handle_neighbor_gone();
            return false;
        }
        else
            return true; // discard, no further processing
    }
    
    switch(state) {

        case WAIT_NB:

            if (ht->hf() == Prophet::SYN)
            {
                update_peer_verifier(pt->sender_instance());
                enqueue_hello(Prophet::SYNACK,
                              pt->transaction_id(),
                              Prophet::Success);
                send_prophet_tlv();
                set_state(SYNRCVD);
                return true;
            }
            return false;

        case SYNSENT:

            if (ht->hf() == Prophet::SYNACK)
            {
                if (hello_c)
                {
                    update_peer_verifier(pt->sender_instance());
                    enqueue_hello(Prophet::ACK,
                                  pt->transaction_id(),
                                  Prophet::Success);
                    send_prophet_tlv();
                    set_state(ESTAB);
                }
                else
                {
                    enqueue_hello(Prophet::RSTACK,
                                  pt->transaction_id(),
                                  Prophet::Failure);
                    send_prophet_tlv();
                    set_state(SYNSENT);
                }
            }
            else
            if (ht->hf() == Prophet::SYN) 
            {
                update_peer_verifier(pt->sender_instance());
                enqueue_hello(Prophet::SYNACK,
                              pt->transaction_id(),
                              Prophet::Success);
                send_prophet_tlv();
                set_state(SYNRCVD);
            }
            else
            if (ht->hf() == Prophet::ACK)
            {
                enqueue_hello(Prophet::RSTACK,
                              pt->transaction_id(),
                              Prophet::Failure);
                send_prophet_tlv();
                set_state(SYNSENT);
            }

            return true;

        case SYNRCVD:

            if (ht->hf() == Prophet::SYNACK)
            {
                if (hello_c) 
                {
                    update_peer_verifier(pt->sender_instance());
                    enqueue_hello(Prophet::ACK,
                                  pt->transaction_id(),
                                  Prophet::Success);
                    send_prophet_tlv();
                    set_state(ESTAB);
                }
                else
                {
                    enqueue_hello(Prophet::RSTACK,
                                  pt->transaction_id(),
                                  Prophet::Failure);
                    send_prophet_tlv();
                    set_state(SYNRCVD);
                }
            }
            else
            if (ht->hf() == Prophet::SYN)
            {
                update_peer_verifier(pt->sender_instance());
                enqueue_hello(Prophet::SYNACK,
                              pt->transaction_id(),
                              Prophet::Success);
                send_prophet_tlv();
                set_state(SYNRCVD);
            }
            else
            if (ht->hf() == Prophet::ACK)
            {
                if (hello_b && hello_c) 
                {
                    enqueue_hello(Prophet::ACK,
                                  pt->transaction_id(),
                                  Prophet::Success);
                    send_prophet_tlv();
                    set_state(ESTAB);
                }
                else
                {
                    enqueue_hello(Prophet::RSTACK,
                                  pt->transaction_id(),
                                  Prophet::Failure);
                    send_prophet_tlv();
                    set_state(SYNRCVD);
                }
            }
            
            return true;

        case ESTAB:

            if (ht->hf() == Prophet::SYN || ht->hf() == Prophet::SYNACK)
            {
                // Section 5.2.1, Note 2: No more than two ACKs should be
                // sent within any time period of length defined by the timer.
                // Thus, one ACK MUST be sent every time the timer expires.
                // In addition, one further ACK may be sent between timer
                // expirations if the incoming message is a SYN or SYNACK.
                // This additional ACK allows the Hello functions to reach
                // synchronisation more quickly.
                if (ack_count_ < 2)
                {
                    ++ack_count_;
                    enqueue_hello(Prophet::ACK,
                                  pt->transaction_id(),
                                  Prophet::Success);
                    send_prophet_tlv();
                }
            }
            else
            if (ht->hf() == Prophet::ACK)
            {
                if (hello_b && hello_c)
                {
                    // Section 5.2.1, Note 3: No more than one ACK should
                    // be sent within any time period of length defined by
                    // the timer.
                    if (ack_count_ < 1) 
                    {
                        ++ack_count_;
                        enqueue_hello(Prophet::ACK,
                                      pt->transaction_id(),
                                      Prophet::Success);
                        send_prophet_tlv();
                    }
                }
                else
                {
                    enqueue_hello(Prophet::RSTACK,
                                  pt->transaction_id(),
                                  Prophet::Failure);
                    send_prophet_tlv();
                }
            }
            return true;

        case WAIT_DICT:
        case WAIT_RIB:
        case OFFER:

            if (ht->hf() == Prophet::ACK)
            {
                set_state(WAIT_DICT);
                return true;
            }
            return false;

        case CREATE_DR:
        case SEND_DR:

            return false;

        case REQUEST:

            if (ht->hf() == Prophet::ACK)
            {
                set_state(CREATE_DR);
                return true;
            }
            return false;

        default:
            if (ht->hf() == Prophet::ACK)
            {
                return true;
            }
            log_err("Unrecognized state %s and HF %d",
                    state_to_str(state),ht->hf());
            return false;
    }

    return false;
}

bool
ProphetEncounter::handle_bad_protocol(u_int32_t tid)
{
    log_debug("handle_bad_protocol");

    // Section 5.2, Note 1: No more than two SYN or SYNACK messages should
    // be sent within any time period of length defined by the timer.
    oasys::Time now;
    now.get_time();
    if ((now - data_sent_).in_milliseconds() < (timeout_ / 2))
    {
        log_debug("flow control engaged, skipping");
        return false;
    }

    prophet_state_t state = get_state("handle_bad_protocol");
    if (state == SYNSENT)
    {
        enqueue_hello(Prophet::SYN,
                      tid, Prophet::Failure);
        send_prophet_tlv();
    }
    else
    if (state == SYNRCVD)
    {
        enqueue_hello(Prophet::SYNACK,
                      tid, Prophet::Failure);
        send_prophet_tlv();
    }
    return false;
}

bool
ProphetEncounter::handle_ribd_tlv(RIBDTLV* rt,ProphetTLV* pt)
{
    log_debug("handle_ribd_tlv");
    (void)pt;

    prophet_state_t state = get_state("handle_ribd_tlv");
    if (state == WAIT_DICT ||
        state == WAIT_RIB)
    {
        if (state == WAIT_DICT)
        {
            reset_ribd("handle_ribd_tlv(WAIT_DICT)");
        }
        ProphetDictionary remote = rt->ribd();
        log_debug("handle_ribd_tlv has %zu entries",remote.size());
        oasys::StringBuffer buf("handle_ribd_tlv\n");
        for(ProphetDictionary::const_iterator i = remote.begin();
            i != remote.end();
            i++)
        {
            u_int16_t sid = (*i).first;
            EndpointID eid = Prophet::eid_to_routeid((*i).second);
            ASSERTF(ribd_.assign(eid,sid) == true,
                    "failed to assign %d to %s",sid,eid.c_str());
        }
        ribd_.dump(&buf);
        log_debug("\n%s\n",buf.c_str());
        set_state(WAIT_RIB);
        return true;
    }
    else
    if (state == OFFER)
    {
        // resend bundle offer
        send_bundle_offer();
        return true;
    }

    return false;
}

bool
ProphetEncounter::handle_rib_tlv(RIBTLV* rt,ProphetTLV* pt)
{
    log_debug("handle_rib_tlv");
    (void)pt;

    EndpointID remote = Prophet::eid_to_routeid(next_hop_->remote_eid());
    prophet_state_t state = get_state("handle_rib_tlv");
    if (state == WAIT_RIB)
    {
        RIBTLV::List rib = rt->nodes();
        log_debug("handle_rib_tlv has %zu entries",rib.size());

        double peer_pvalue = 0.0;

        // look up previous information on peer
        ProphetNode* node = oracle_->nodes()->find(remote);

        // else create new node
        if (node == NULL)
        {
            node = new ProphetNode(oracle_->params());
            node->set_eid(remote);
        }

        node->set_relay(rt->relay_node());
        node->set_custody(rt->custody_node());
        node->set_internet_gw(rt->internet_gateway());
        
        // apply direct contact algorithm
        node->update_pvalue();
        peer_pvalue = node->p_value();

        // update node table
        oracle_->nodes()->update(node);

        // now iterate over the rest of the RIB
        for(RIBTLV::iterator i = rib.begin();
            i != rib.end();
            i++)
        {
            RIBNode* rib = (*i);
            u_int16_t sid = rib->sid_;
            EndpointID eid = Prophet::eid_to_routeid(ribd_.find(sid));
            ASSERT(eid.equals(EndpointID::NULL_EID()) == false);
            
            // look up previous information
            node = oracle_->nodes()->find(eid);

            // else create new node
            if (node == NULL)
            {
                node = new ProphetNode(oracle_->params());
                node->set_eid(eid);
            }
            ASSERT(
               node->remote_eid().equals(EndpointID::NULL_EID()) == false
            );
            node->set_relay(rib->relay());
            node->set_custody(rib->custody());
            node->set_internet_gw(rib->internet_gw());

            // apply transitive contact algorithm
            node->update_transitive(peer_pvalue,rib->p_value());

            // update node table
            oracle_->nodes()->update(node);

            // keep mirror copy of remote's RIB
            ProphetNode* rn = new ProphetNode(*node);
            rn->set_pvalue(rib->p_value());
            remote_nodes_.update(rn);
        }

        set_state(OFFER);
        
        return true;
    }
    
    return false;
}

bool
ProphetEncounter::handle_bundle_tlv(BundleTLV* bt,ProphetTLV* pt)
{
    log_debug("handle_bundle_tlv");

    prophet_state_t state = get_state("handle_bundle_tlv");
    BundleList list("handle_bundle_tlv");
    if (state == WAIT_RIB)
    {
        enqueue_hello(Prophet::ACK,
                      pt->transaction_id(),
                      Prophet::Failure);
        send_prophet_tlv();
        set_state(WAIT_DICT);
        return false;
    }
    else
    if (state == OFFER)
    {
        // grab a list of Bundles from main Prophet store
        oasys::ScopeLock obl(oracle_->bundles()->lock(),
                             "ProphetEncounter::handle_bundle_tlv");
        oracle_->bundles()->bundle_list(list);

        // pull in the bundle request from this TLV
        size_t num = bt->list().size();
        log_debug("handle_bundle_tlv(OFFER) received list of %zu elements",
                  num);

        // list size 0 has special meaning
        if (num == 0)
        {
            set_state(WAIT_INFO);
        }
        else
        if (list.size() > 0)
        {
            // Pull off the Bundle request received from peer
            BundleOfferList br_rcvd(BundleOffer::RESPONSE);

            oasys::ScopeLock brl(br_rcvd.lock(),"handle_bundle_tlv");
            br_rcvd = bt->list();

            // Walk down each request and attempt to fulfill
            for (BundleOfferList::iterator i = br_rcvd.begin();
                i != br_rcvd.end(); i++)
            {
                BundleOffer* bo = (*i);
                ASSERT (bo != NULL);
                u_int32_t cts = (*i)->creation_ts();
                u_int32_t seq = (*i)->seqno();
                u_int16_t sid = (*i)->sid();
                EndpointID eid = ribd_.find(sid);

                ASSERTF(eid.equals(EndpointID::NULL_EID()) == false,
                        "null eid found for sid %d",sid);

                // find any Bundles with destination that matches
                // this routeid and with creation ts that matches cts
                BundleRef b("handle_bundle_tlv");
                b = ProphetBundleList::find(list,eid,cts,seq);

                if (b.object() != NULL)
                {
                    // check link status, forwarding log, etc
                    if(should_fwd(b.object()))
                    {
                        // enqueue to send over the link to peer
                        fwd_to_nexthop(b.object());

                    }
                }
                else
                {
                    log_info("bundle not found: peer requested %s (%u:%u)",
                             eid.c_str(),cts,seq);
                }
            }

        }
        return true;
    }
    else
    if (state == SEND_DR)
    {
        // grab list of bundles
        oasys::ScopeLock obl(oracle_->bundles()->lock(),
                           "ProphetEncounter::handle_bundle_tlv");
        oracle_->bundles()->bundle_list(list);

        // read out the Bundle offer from the TLV
        offers_ = bt->list();
        log_debug("handle_bundle_tlv(SEND_DR) received list of %zu elements",
                   offers_.size());
        ASSERT(offers_.type() != BundleOffer::RESPONSE);

        // prepare a new request
        requests_.clear();
        requests_.set_type(BundleOffer::RESPONSE);

        oasys::ScopeLock l2(offers_.lock(),"handle_bundle_tlv");
        for (BundleOfferList::iterator i = offers_.begin();
             i != offers_.end();
             i++)
        {
            u_int32_t cts = (*i)->creation_ts();
            u_int32_t seq = (*i)->seqno();
            u_int16_t sid = (*i)->sid();
            EndpointID eid = ribd_.find(sid);

            ASSERTF(eid.equals(EndpointID::NULL_EID()) == false,
                    "failed on eid lookup for sid %d",sid);

            if ((*i)->ack())
            {
                u_int32_t ets = 0;
                // must delete any ACK'd bundles
                BundleRef b("handle_bundle_tlv");
                b = ProphetBundleList::find(list,eid,cts,seq);
                if (b.object() != NULL)
                {
                    ets = b->expiration_;
                    oracle_->bundles()->drop_bundle(b.object());
                    oracle_->actions()->delete_bundle(b.object(),
                            BundleProtocol::REASON_NO_ADDTL_INFO);
                    list.erase(b.object());
                }
                
                // preserve this ACK for future encounters
                oracle_->acks()->insert(eid,cts,seq,ets);
            }
            else
            // only request if I don't already have it!
            if (ProphetBundleList::find(list,eid,cts,seq) == NULL)
            {
                bool accept = true;
                //XXX/wilson
                // need to to something intelligent here w.r.t. custody
                bool custody = false;
                requests_.add_offer(cts,seq,sid,custody,accept,false);
                log_debug("requesting bundle, cts:seq %d:%d, sid %d",
                          cts,seq,sid);
            }
        }
        // request in order of most likely to deliver
        requests_.sort(&ribd_,oracle_->nodes(),synsender_ ? 0 : 1);
        ASSERT(requests_.type() == BundleOffer::RESPONSE);
        enqueue_bundle_tlv(requests_,
                           pt->transaction_id(),
                           Prophet::Success);
        send_prophet_tlv();
        set_state(REQUEST);
        if(requests_.size() == 0)
        {
            set_state(WAIT_INFO);
        }
    }
    return false;
}

void
ProphetEncounter::handle_neighbor_gone()
{
    log_debug("handle_neighbor_gone");
    neighbor_gone_ = true;
    log_info("*%p - %u received NEIGHBOR_GONE signal",
             next_hop_.object(),local_instance_);
}

void
ProphetEncounter::handle_poll_timeout()
{
    log_debug("handle_poll_timeout");

    prophet_state_t state = get_state("handle_poll_timeout");
    switch(state) {
        case SYNSENT:
        case WAIT_NB:
            if (synsender_ == true)
            {
                enqueue_hello(Prophet::SYN);
                send_prophet_tlv();
            }
            break;
        case SYNRCVD:
            enqueue_hello(Prophet::SYNACK,
                          tid_,
                          Prophet::Success);
            send_prophet_tlv();
            break;
        case ESTAB:
            // Section 5.2.1, Note 2: No more than two ACKs should be
            // sent within any time period of length defined by the timer.
            // Thus, one ACK MUST be sent every time the timer expires.
            // In addition, one further ACK may be sent between timer
            // expirations if the incoming message is a SYN or SYNACK.
            // This additional ACK allows the Hello functions to reach
            // synchronisation more quickly.
            if (ack_count_ >= 2)
            {
                ack_count_ = 0;
            }
            else 
            {
                ++ack_count_;
                enqueue_hello(Prophet::ACK);
                send_prophet_tlv();
            }
            break;
        case WAIT_DICT:
            enqueue_hello(Prophet::ACK,
                          tid_,
                          Prophet::Success);
            send_prophet_tlv();
            break;
        case WAIT_RIB:
            enqueue_hello(Prophet::ACK,
                          tid_,
                          Prophet::Success);
            send_prophet_tlv();
            set_state(WAIT_DICT);
            break;
        case CREATE_DR:
            send_dictionary();
            set_state(SEND_DR);
            break;
        case SEND_DR:
            send_dictionary();
            break;
        case REQUEST:
            if(requests_.size() == 0)
            {
                set_state(WAIT_INFO);
            }
            else
            {
                enqueue_bundle_tlv(requests_,
                                   tid_,
                                   Prophet::Success);
                send_prophet_tlv();
            }
            break;
        case OFFER:
            send_bundle_offer();
            break;
        case WAIT_INFO:
            // After switching from 1st phase (see set_state), wait for
            // 1/2 hello_dead before switching phases
            {
                oasys::Time now;
                now.get_time();

                u_int timeout = oracle_->params()->hello_dead_ * timeout_ / 2;
                u_int diff = (now - data_sent_).in_milliseconds();
                if ( diff <= timeout )
                {
                    log_debug("wait_info: timediff %u timeout %u",
                              diff,timeout);
                }
                else
                {
                    switch_info_role();
                }
            }
            break;
        default:
            break;
    }
}

void
ProphetEncounter::reset_ribd(const char* where)
{
    EndpointID local(BundleDaemon::instance()->local_eid()),
               remote(next_hop_->remote_eid());
    log_debug("resetting Prophet dictionary (%s)",where);
    ribd_.clear();
    if (synsender_ == true)
    { 
        ASSERT(ribd_.assign(local,0));
        ASSERT(ribd_.assign(remote,1));
    }
    else
    {
        ASSERT(ribd_.assign(local,1));
        ASSERT(ribd_.assign(remote,0));
    }
}

void
ProphetEncounter::switch_info_role()
{
    ASSERT(get_state("switch_info_role") == WAIT_INFO);
    if (synsender_ == true)
    {
        if (initiator_ == true)
        {
            initiator_ = false;
            set_state(WAIT_DICT);
        }
        else // initiator_ == false
        {
            initiator_ = true;
            set_state(CREATE_DR);
        }
    }
    else // synsender_ == false
    {
        if (initiator_ == false)
        {
            initiator_ = true;
            set_state(CREATE_DR);
        }
        else // initiator_ == true
        {
            initiator_ = false;
            set_state(WAIT_DICT);
        }
    }
}

void
ProphetEncounter::dump_state(oasys::StringBuffer* buf)
{
    buf->appendf("%05d  %10s  %s\n",local_instance_,
                 state_to_str(state_),
                 next_hop_->remote_eid().c_str());
}

void
ProphetEncounter::send_bundle_offer()
{
    log_debug("send_bundle_offer");
    ASSERT(oracle_ != NULL);
    ASSERT(oracle_->bundles() != NULL);

    prophet_state_t state = get_state("send_bundle_offer");
    ASSERT( state == WAIT_RIB ||
            state == OFFER );

    // reset to sanity
    offers_.clear();
    offers_.set_type(BundleOffer::OFFER);

    // Grab a list of Bundles from queueing policy enforcer
    oasys::ScopeLock l(oracle_->bundles()->lock(),
                       "ProphetEncounter::send_bundle_offer");
    BundleList list("send_bundle_offer");
    oracle_->bundles()->bundle_list(list);

    if (list.size() > 0)
    {
        // Create ordering based on priority_queue using forwarding strategy 
        FwdStrategy* fs = FwdStrategy::strategy(
                                oracle_->params()->fs_,
                                oracle_->nodes(),
                                &remote_nodes_);

        // Create strategy-based decider for whether to forward a bundle
        ProphetDecider* d = ProphetDecider::decider(
                                oracle_->params()->fs_,
                                oracle_->nodes(),
                                &remote_nodes_,
                                next_hop_,
                                oracle_->params()->max_forward_,
                                oracle_->stats());

        // Combine into priority_queue for bundle offer ordering
        ProphetBundleOffer offer(list,fs,d);

        while(!offer.empty())
        {
            BundleRef b("send_bundle_offer");
            b = offer.top();
            offer.pop();
            if (b.object() == NULL)
                continue;

            EndpointID eid = Prophet::eid_to_routeid(b->dest_);
            u_int16_t sid = ribd_.find(eid);

            ASSERTF(sid != ProphetDictionary::INVALID_SID,
                    "failed on dictionary lookup for %s",eid.c_str());

            if (sid == 0) 
            {
                // if sid is assigned in this ribd, it had better be the
                // synsender (protocol initiator).
                // if sid is not assigned in this ribd, then it's an error
                // since this phase of the protocol cannot send RIBD updates
                ASSERT(ribd_.is_assigned(eid) == true && synsender_ == true);
                continue;
            }
            else
            if (sid == 1 && synsender_ == false)
            {
                // local destination, don't offer
                continue;
            }

            // add bundle listing to the offer
            offers_.add_offer(b.object(),sid);
            log_debug("offering bundle *%p (cts:seq %d:%d,sid %d)",
                      b.object(),b.object()->creation_ts_.seconds_,
                      b.object()->creation_ts_.seqno_,sid);
        }

        delete fs;
        delete d;
    }

    // Now append all known ACKs
    PointerList<ProphetAck> acklist;
    oracle_->acks()->fetch(EndpointIDPattern("dtn://*"),acklist);

    for (PointerList<ProphetAck>::iterator i = acklist.begin();
         i != acklist.end();
         i++)
    {
        ProphetAck* pa = *i;

        EndpointID eid = Prophet::eid_to_routeid(pa->dest_id_);
        u_int16_t sid = ribd_.find(eid);

        // can't find this EID in the dictionary
        if (sid == ProphetDictionary::INVALID_SID)
        {
            // unable to send ACK during this exchange, skip for now
            continue;
        }

        // don't send ACK to peer if peer is original Bundle's destination
        if ( (synsender_ == true && sid == 1) ||
             (synsender_ != true && sid == 0) )
        {
            continue;
        }
        
        // add ACK to the list
        offers_.add_offer(pa->cts_,pa->seq_,sid,false,false,true);
        log_debug("appending ACK to bundle offer: cts:seq %d:%d, sid %d",
                  pa->cts_,pa->seq_,sid);
    }

    enqueue_bundle_tlv(offers_);
    send_prophet_tlv();
}

void
ProphetEncounter::send_dictionary()
{
    log_debug("send_dictionary");
    ASSERT(oracle_ != NULL);
    ASSERT(oracle_->nodes() != NULL);
    ASSERT(next_hop_ != NULL);

    // list of all known ProphetNodes, indexed by peer endpoint id
    ProphetNodeList nodes;
    // list of predictability values for each node, indexed by string id
    RIBTLV::List rib;
    // EIDs for peer endpoints in this exchange
    EndpointID local(BundleDaemon::instance()->local_eid()),
               remote(next_hop_->remote_eid());

    prophet_state_t state = get_state("send_dictionary");

    // sanity
    if (state == CREATE_DR)
    {
        reset_ribd("send_dictionary(CREATE_DR)");
    }

    // ASSERT protocol invariants
    if (synsender_ == true)
    {
        ASSERT(ribd_.find(local) == 0);
        ASSERT(ribd_.find(remote) == 1);
    }
    else
    {
        ASSERT(ribd_.find(remote) == 0);
        ASSERT(ribd_.find(local) == 1);
    }

    // ask ProphetController for snapshot of master node list
    oracle_->nodes()->dump_table(nodes);
    
    // walk over the master node list and create SIDs for each
    for(ProphetNodeList::iterator i = nodes.begin();
        i != nodes.end();
        i++)
    {
        ProphetNode* node = *i;

        u_int16_t sid;

        if (ribd_.is_assigned(node->remote_eid()))
        {
            sid = ribd_.find(node->remote_eid());
        }
        else
        {
            sid = ribd_.insert(node->remote_eid());
        }

        ASSERT(ribd_.is_assigned(node->remote_eid()));

        if (sid == 0 || sid == 1)
        {
            // implicit peer endpoints
            continue;
        }

        ASSERT(sid != ProphetDictionary::INVALID_SID);
        rib.push_back(new RIBNode(node,sid));
    }

    // ask ProphetController for list of active bundles
    oasys::ScopeLock l(oracle_->bundles()->lock(),
                       "ProphetEncounter::send_dictionary");
    BundleList list("send_dictionary");
    oracle_->bundles()->bundle_list(list);
    oasys::ScopeLock bl(list.lock(),"send_dictionary");
    for (BundleList::iterator i = list.begin();
         i != list.end();
         i++)
    {
        u_int16_t sid;
        EndpointID routeid = Prophet::eid_to_routeid((*i)->dest_);
        if (ribd_.is_assigned(routeid))
            continue;

        // shouldn't get here unless node got truncated
        // (p-value less than epsilon)
        ASSERT((sid = ribd_.insert(routeid)) != 0);
        ProphetNode* node = new ProphetNode(oracle_->params());
        node->set_eid(routeid);
        oracle_->nodes()->update(node);
        rib.push_back(new RIBNode(node,sid));
    }

    u_int32_t tid = Prophet::UniqueID::tid();
    enqueue_ribd(ribd_,tid,Prophet::NoSuccessAck);
    enqueue_rib(rib,tid,Prophet::NoSuccessAck);
    send_prophet_tlv();
}

ProphetTLV*
ProphetEncounter::outbound_tlv(u_int32_t tid,
                               Prophet::header_result_t result)
{
    oasys::ScopeLock l(otlv_lock_,"outbound_tlv");
    if (tid == 0)
    {
        tid = Prophet::UniqueID::tid();
    }
    if (outbound_tlv_ == NULL)
    {
        outbound_tlv_ = new ProphetTLV(result,
                              local_instance_,
                              remote_instance_,
                              tid);
    }
    else
    {
        if (outbound_tlv_->transaction_id() != tid)
        {
            log_err("mismatched tid: TLV %u tid %u",
                    outbound_tlv_->transaction_id(),
                    tid);
            return NULL;
        }

        if (outbound_tlv_->result() != result)
        {
            log_err("mismatched result field: TLV %s result %s",
                    Prophet::result_to_str(outbound_tlv_->result()),
                    Prophet::result_to_str(result));
            return NULL;
        }
    }
    return outbound_tlv_;
}

bool
ProphetEncounter::send_prophet_tlv()
{
    oasys::ScopeLock l(otlv_lock_,"send_prophet_tlv");
    if (neighbor_gone_ == true) return false;
    if (outbound_tlv_ == NULL) return false;

    ASSERT(outbound_tlv_->num_tlv() > 0);

    bool retval = false;
    
    BundleRef b("ProphetEncounter send_prophet_tlv");
    b = new Bundle();

    // append Prophet service tag (in emulation of Registration)
    EndpointID src(BundleDaemon::instance()->local_eid()),
               dst(next_hop_->remote_eid());
    src.append_service_tag("prophet");
    dst.append_service_tag("prophet");

    // encapsulate ProphetTLV into Bundle and queue up
    if (outbound_tlv_->create_bundle(b.object(),src,dst))
    {
        oasys::StringBuffer buf;
        outbound_tlv_->dump(&buf);
        log_debug("Outbound TLV\n");
        log_debug("%s\n",buf.c_str());

        // enqueue before non-control bundles
        fwd_to_nexthop(b.object(),true);

        // update timestamp
        data_sent_.get_time();

        retval = true;
    }
    else
    {
        log_err("Failed to write out ProphetTLV");
        retval = false;
    }

    delete outbound_tlv_;
    outbound_tlv_ = NULL;
    return retval;
}

void
ProphetEncounter::enqueue_hello(Prophet::hello_hf_t hf,
                                u_int32_t tid,
                                Prophet::header_result_t result)
{
    log_debug("enqueue_hello %s %u %s",
               Prophet::hf_to_str(hf), tid,
               Prophet::result_to_str(result));

    oasys::ScopeLock l(otlv_lock_,"enqueue_hello");
    ProphetTLV* tlv = outbound_tlv(tid,result);
    HelloTLV *ht = new HelloTLV(hf,
                                oracle_->params()->hello_interval_,
                                BundleDaemon::instance()->local_eid(),
                                logpath_);

    ASSERT(tlv != NULL);
    tlv->add_tlv(ht);
}

void
ProphetEncounter::enqueue_ribd(const ProphetDictionary& ribd,
                               u_int32_t tid,
                               Prophet::header_result_t result)
{
    log_debug("enqueue_ribd (%zu entries) %u %s",
               ribd.size(),tid,
               Prophet::result_to_str(result));

    oasys::ScopeLock l(otlv_lock_,"enqueue_ribd");
    ProphetTLV* tlv = outbound_tlv(tid,result);
    RIBDTLV *rt = new RIBDTLV(ribd,logpath_);

    ASSERT(tlv != NULL);
    tlv->add_tlv(rt);
}

void
ProphetEncounter::enqueue_rib(const RIBTLV::List& nodes,
                              u_int32_t tid,
                              Prophet::header_result_t result)
{
    log_debug("enqueue_rib (%zu entries)",nodes.size());

    oasys::ScopeLock l(otlv_lock_,"enqueue_rib");
    ProphetTLV* tlv = outbound_tlv(tid,result);
    RIBTLV *rt = new RIBTLV(nodes,
                            oracle_->params()->relay_node_,
                            oracle_->params()->custody_node_,
                            oracle_->params()->internet_gw_,
                            logpath_);

    ASSERT(tlv != NULL);
    tlv->add_tlv(rt);
}

void
ProphetEncounter::enqueue_bundle_tlv(const BundleOfferList& bundles,
                                     u_int32_t tid,
                                     Prophet::header_result_t result)
{
    log_debug("enqueue_bundle_tlv (%zu entries)",bundles.size());

    oasys::ScopeLock l(otlv_lock_,"enqueue_bundle_tlv");
    ProphetTLV* tlv = outbound_tlv(tid,result);
    BundleTLV* bt = new BundleTLV(bundles,logpath_);

    ASSERT(tlv != NULL);
    tlv->add_tlv(bt);
}

void
ProphetEncounter::process_command()
{
    PEMsg msg;
    bool ok = cmdqueue_.try_pop(&msg);

    // shouldn't get here unless a message is waiting
    ASSERT( ok == true );

    // dispatch command
    switch(msg.type_) {
        case PEMSG_PROPHET_TLV_RECEIVED:
            log_debug("processing PEMSG_PROPHET_TLV_RECEIVED");
            handle_prophet_tlv(msg.tlv_);
            delete msg.tlv_;
            break;
        case PEMSG_NEIGHBOR_GONE:
            log_debug("processing PEMSG_NEIGHBOR_GONE");
            handle_neighbor_gone();
            break;
        case PEMSG_HELLO_INTERVAL_CHANGED:
            log_debug("processing PEMSG_HELLO_INTERVAL_CHANGED");
            handle_hello_interval_changed();
            break;
        default:
            PANIC("invalid PEMsg typecode %d",msg.type_);
    }
}

void
ProphetEncounter::handle_hello_interval_changed() {
    timeout_ = oracle_->params()->hello_interval_ * 100;
}

void
ProphetEncounter::run() {

    ASSERT(oracle_ != NULL); 
    EndpointID local(BundleDaemon::instance()->local_eid()),
               remote(next_hop_->remote_eid());
    
    //XXX protocol seems broken, need a tie-breaker 
    // tie-breaker for Hello sequence
    synsender_ = (local.str() < remote.str());

    // who starts the oscillation for Information Exchange
    // (this toggles back and forth for the duration of the peering)
    initiator_ = synsender_;

    log_debug("synsender_ == %s", synsender_ ? "true" : "false");

    remote_nodes_.clear();
    if (synsender_ == true)
    { 
        enqueue_hello(Prophet::SYN);
        send_prophet_tlv();
        set_state(SYNSENT);
    }

    /*
       ProphetEncounter lives for the duration of an encounter between
       two Prophet nodes.  First the Hello sequence is completed, then
       the Information Exchange phase.  As soon as inactivity lasts 
       beyond HELLO_DEAD*HELLO_INTERVAL seconds, this thread goes away
       and all its state with it.  The RIB exchange persists in that 
       ProphetController keeps a master list of Prophet nodes. 
    */

    u_int timeout = timeout_;
    while (neighbor_gone_ == false) {

        if (cmdqueue_.size() != 0)
        {
            process_command();
            continue;
        }

        // mix up jitter on timeout to +/- 5%
        // so much for limiting FP math eh?
        int r = oasys::Random::rand(10);
        double ratio = (double) (10 - r) / (double) 100;
        timeout = (int) ((double) timeout_ * (1.05 - ratio));
        log_debug("poll timeout = %d", timeout);

        short revents = 0;
        int cc = oasys::IO::poll_single(cmdqueue_.read_fd(),
                                        POLLIN,&revents,timeout);

        if (neighbor_gone_ == true) break;
        if (cc == oasys::IOTIMEOUT)
        {
            handle_poll_timeout();
        }
        else if (cc > 0)
        {
            // flip back 'round to process_command();
            continue;
        }
        else
        {
            log_err("unexpected return on poll: %d",cc);
            handle_neighbor_gone();
            break;
        } 

        oasys::Time now;
        now.get_time();
        if ((now-data_rcvd_).in_milliseconds() >= (oracle_->params()->hello_dead_ * timeout_)) {
            log_err("%d silent Hello intervals, giving up",oracle_->params()->hello_dead_);
            break;
        }
    }

    ProphetController::instance()->unreg(this);
}

} // namespace dtn

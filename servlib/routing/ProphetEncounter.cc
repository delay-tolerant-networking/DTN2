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

#include <queue>

namespace dtn {

ProphetEncounter::ProphetEncounter(Link* nexthop,
                                   ProphetOracle* oracle)
    : oasys::Thread("ProphetEncounter",oasys::Thread::DELETE_ON_EXIT),
      oasys::Logger("ProphetEncounter","/dtn/route"),
      oracle_(oracle),
      remote_instance_(0),
      local_instance_(Prophet::UniqueID::instance()->instance_id()),
      deadcount_(0),
      tid_(0),
      timeout_(0),
      next_hop_(nexthop),
      synsender_(false),initiator_(false),
      synsent_(false),synrcvd_(false),
      estab_(false),
      neighbor_gone_(false),
      state_(WAIT_NB),
      cmdqueue_("/dtn/route"),
      to_be_fwd_("ProphetEncounter to be forwarded"),
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
    ::gettimeofday(&data_rcvd_,0);
    ::gettimeofday(&data_sent_,0);
    
    // initialize with "permission" to send
    // convert from ms to sec
    data_sent_.tv_sec -= ( (timeout_/1000) + 1 );
    
    // initialize lists
    offers_.set_type(BundleOffer::OFFER);
    requests_.set_type(BundleOffer::RESPONSE);
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
        deadcount_ = 0;
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
        send_bundle_offer();
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
ProphetEncounter::handle_bundle_received(Bundle* b)
{
    log_debug("handle_bundle_received *%p",b);

    // comment out this shortcut until after I verify
    // the protocol "could" route the same message
#if 0
    // Is this bundle destined for the attached peer?
    EndpointIDPattern route = Prophet::eid_to_route(next_hop_->remote_eid());
    if (route.match(b->dest_))
    {
        // destined for peer ... now decide whether to forward it
        ProphetDecider* d = ProphetDecider::decider( oracle_->params()->fs_,
                                oracle_->nodes(), &remote_nodes_, next_hop_,
                                oracle_->params()->max_forward_,
                                oracle_->stats());

        // short-circuit the protocol and forward to attached peer
        if (d->should_fwd(b))
        {
            fwd_to_nexthop(b);
        }

        delete d;
    }
#endif

    if (get_state("handle_bundle_received") == REQUEST)
    {
        u_int32_t cts = b->creation_ts_.seconds_;
        u_int16_t sid = ribd_.find(Prophet::eid_to_routeid(b->dest_));
        requests_.remove_bundle(cts,sid);

        // no requests remaining means the end of bundle exchange
        // so signal as such with 0-sized bundle request
        if (requests_.size() == 0)
        {
            enqueue_bundle_tlv(requests_,
                    Prophet::UniqueID::tid(),
                    Prophet::NoSuccessAck);
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
    // update timer
    ::gettimeofday(&data_rcvd_,0);
}

void
ProphetEncounter::neighbor_gone()
{
    log_debug("neighbor_gone");
    // alert thread to status change
    cmdqueue_.push_back(PEMsg(PEMSG_NEIGHBOR_GONE,NULL));
}

void
ProphetEncounter::fwd_to_nexthop(Bundle* bundle,bool add_front)
{
    log_debug("fwd_to_nexthop *%p at %s",bundle,add_front?"front":"back");

    // ProphetEncounter only exists if link is open
    ASSERT(next_hop_->isopen());

    if(bundle!=NULL)
    {
        if(add_front)
            //XXX/wilson this is naive, if more than one
            // gets enqueued we're busted
            
            // give priority to Prophet control messages
            to_be_fwd_.push_front(bundle);
        else
            to_be_fwd_.push_back(bundle);
    }

    // fill the pipe with however many bundles are pending
    while (next_hop_->isbusy() == false &&
           to_be_fwd_.size() > 0)
    {
        BundleRef ref("ProphetEncounter fwd_to_nexthop");
        ref = to_be_fwd_.pop_front();

        if (ref.object() == NULL)
        {
            ref = NULL;
            break;
        }

        Bundle* b = ref.object();
        log_debug("sending *%p to *%p", b, next_hop_);

        bool ok = oracle_->actions()->send_bundle(b,next_hop_,
                                                  ForwardingInfo::COPY_ACTION,
                                                  CustodyTimerSpec());
        ASSERTF(ok == true,"failed to send bundle");

        // update timestamp
        ::gettimeofday(&data_sent_,0);

        // update Prophet stats on this bundle
        oracle_->stats()->update_stats(b,remote_nodes_.p_value(b));
    } 

    oasys::ScopeLock l(to_be_fwd_.lock(),"fwd_to_nexthop");
    for(BundleList::iterator i = to_be_fwd_.begin();
            i != to_be_fwd_.end(); i++)
    {
        log_debug("can't forward *%p to *%p because link is busy",
                   *i, link);
    }
}

void 
ProphetEncounter::handle_prophet_tlv(ProphetTLV* pt)
{
    ASSERT(pt != NULL);
    log_debug("handle_prophet_tlv (tid %d,%s)",
               pt->transaction_id(),
               Prophet::result_to_str(pt->result()));

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
                delete (HelloTLV*) tlv;
                break;
            case Prophet::RIBD_TLV:
                ok = handle_ribd_tlv((RIBDTLV*)tlv,pt);
                delete (RIBDTLV*) tlv;
                break;
            case Prophet::RIB_TLV:
                ok = handle_rib_tlv((RIBTLV*)tlv,pt);
                delete (RIBTLV*) tlv;
                break;
            case Prophet::BUNDLE_TLV:
                ok = handle_bundle_tlv((BundleTLV*)tlv,pt);
                delete (BundleTLV*) tlv;
                break;
            case Prophet::UNKNOWN_TLV:
            case Prophet::ERROR_TLV:
            default:
                PANIC("Unexpected TLV type received by ProphetEncounter: %d",
                      tlv->typecode());
        }
    }
    if (ok == true) 
    {
        deadcount_ = 0;
    }
    else
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

    log_info("received HF %s in state %s",Prophet::hf_to_str(ht->hf()),
             state_to_str(get_state("handle_hello_tlv")));

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
    
    prophet_state_t state = get_state("handle_hello_tlv");
    switch(state) {

        case WAIT_NB:

            if (ht->hf() == Prophet::SYN)
            {
                update_peer_verifier(pt->sender_instance());
                enqueue_hello(Prophet::SYNACK,
                              pt->transaction_id(),
                              Prophet::Success);
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
                    set_state(ESTAB);
                }
                else
                {
                    enqueue_hello(Prophet::RSTACK,
                                  pt->transaction_id(),
                                  Prophet::Failure);
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
                set_state(SYNRCVD);
            }
            else
            if (ht->hf() == Prophet::ACK)
            {
                enqueue_hello(Prophet::RSTACK,
                              pt->transaction_id(),
                              Prophet::Failure);
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
                    set_state(ESTAB);
                }
                else
                {
                    enqueue_hello(Prophet::RSTACK,
                                  pt->transaction_id(),
                                  Prophet::Failure);
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
                    set_state(ESTAB);
                }
                else
                {
                    enqueue_hello(Prophet::RSTACK,
                                  pt->transaction_id(),
                                  Prophet::Failure);
                    set_state(SYNRCVD);
                }
            }
            
            return true;

        case ESTAB:

            if (ht->hf() == Prophet::SYN || ht->hf() == Prophet::SYNACK)
            {
                enqueue_hello(Prophet::ACK,
                              pt->transaction_id(),
                              Prophet::Success);
            }
            else
            if (ht->hf() == Prophet::ACK)
            {
                if (hello_b && hello_c)
                {
                    enqueue_hello(Prophet::ACK,
                                  pt->transaction_id(),
                                  Prophet::Success);
                }
                else
                {
                    enqueue_hello(Prophet::RSTACK,
                                  pt->transaction_id(),
                                  Prophet::Failure);
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
                ::gettimeofday(&data_rcvd_,0);
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

    prophet_state_t state = get_state("handle_bad_protocol");
    if (state == SYNSENT)
    {
        enqueue_hello(Prophet::SYN,
                      tid, Prophet::Failure);
    }
    else
    if (state == SYNRCVD)
    {
        enqueue_hello(Prophet::SYNACK,
                      tid, Prophet::Failure);
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
        ProphetDictionary remote = rt->ribd();
        log_debug("handle_ribd_tlv has %zu entries",remote.size());
        oasys::StringBuffer buf("handle_ribd_tlv\n");
        for(ProphetDictionary::const_iterator i = remote.begin();
            i != remote.end();
            i++)
        {
            u_int16_t sid = (*i).first;
            EndpointID eid = Prophet::eid_to_routeid((*i).second);
            ASSERT(ribd_.assign(eid,sid) == true);
            buf.appendf("%04d: %s\n",sid,eid.c_str());
        }
        log_debug(buf.c_str());
        set_state(WAIT_RIB);
        return true;
    }
    else
    if (state == OFFER)
    {
        // resend bundle offer
        set_state(OFFER);
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
            node = oracle_->nodes()->find(eid);
            if (node == NULL)
            {
                node = new ProphetNode(oracle_->params());
                node->set_eid(eid);
            }
            node->set_relay(rib->relay());
            node->set_custody(rib->custody());
            node->set_internet_gw(rib->internet_gw());

            // apply transitive contact algorithm
            node->update_transitive(peer_pvalue,rib->p_value());

            // update node table
            oracle_->nodes()->update(node);
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
    if (state == WAIT_RIB)
    {
        enqueue_hello(Prophet::ACK,
                      pt->transaction_id(),
                      Prophet::Failure);
        set_state(WAIT_DICT);
        return false;
    }
    else
    if (state == OFFER)
    {
        // grab a list of Bundles from main Prophet store
        BundleVector vec;
        oracle_->bundles()->vector(vec);

        // pull in the bundle request from this TLV
        ASSERT(requests_.type() == BundleOffer::RESPONSE);
        requests_ = bt->list();
        log_debug("handle_bundle_tlv(OFFER) received list of %zu elements",
                   requests_.size());

        // list size 0 has special meaning
        if (requests_.size() == 0)
        {
            set_state(WAIT_INFO);
        }
        else
        {
            oasys::ScopeLock l(requests_.lock(),"handle_bundle_tlv");
            for (BundleOfferList::iterator i = requests_.begin();
                i != requests_.end();
                i++)
            {
                u_int32_t cts = (*i)->creation_ts();
                u_int16_t sid = (*i)->sid();
                EndpointID eid = ribd_.find(sid);

                ASSERT(eid.equals(EndpointID::NULL_EID()) == false);

                // find any Bundles with destination that matches
                // this routeid and with creation ts that matches cts
                Bundle* b = vec.find(eid,cts);

                                 // don't send ACK'd bundles
                if (b != NULL && !oracle_->acks()->is_ackd(b)) {

                    // enqueue to send over the link to peer
                    fwd_to_nexthop(b);

                    // remove from list so as only to forward once
                    requests_.remove_bundle(cts,sid);
                }
            }

            set_state(OFFER);
        }
        return true;
    }
    else
    if (state == SEND_DR)
    {
        // grab list of bundles
        BundleVector vec;
        oracle_->bundles()->vector(vec);

        // read out the Bundle offer from the TLV
        ASSERT(offers_.type() == BundleOffer::OFFER);
        offers_ = bt->list();
        log_debug("handle_bundle_tlv(SEND_DR) received list of %zu elements",
                   requests_.size());

        // prepare a new request
        requests_.clear();
        requests_.set_type(BundleOffer::RESPONSE);

        oasys::ScopeLock l(offers_.lock(),"handle_bundle_tlv");
        for (BundleOfferList::iterator i = offers_.begin();
             i != offers_.end();
             i++)
        {
            u_int32_t cts = (*i)->creation_ts();
            u_int16_t sid = (*i)->sid();
            EndpointID eid = ribd_.find(sid);

            ASSERTF(eid.equals(EndpointID::NULL_EID()) == false,
                    "failed on eid lookup for sid %d",sid);

            if ((*i)->ack())
            {
                u_int32_t ets = 0;
                // must delete any ACK'd bundles
                Bundle* b = vec.find(eid,cts);
                if (b != NULL)
                {
                    ets = b->expiration_;
                    oracle_->bundles()->drop_bundle(b);

                    // list is now invalid, reload!
                    oracle_->bundles()->vector(vec);
                }
                
                // preserve this ACK for future encounters
                oracle_->acks()->insert(eid,cts,ets);
            }
            else
            // don't request if I already have it!
            if (vec.find(eid,cts) == NULL)
            {
                bool accept = true;
                //XXX/wilson
                // need to to something intelligent here w.r.t. custody
                bool custody = false;
                requests_.push_back(
                            new BundleOffer(cts,sid,
                                    custody,accept,false,
                                    BundleOffer::RESPONSE));
            }
        }
        // request in order of most likely to deliver
        requests_.sort(&ribd_,oracle_->nodes(),synsender_ ? 0 : 1);
        enqueue_bundle_tlv(requests_,
                           pt->transaction_id(),
                           Prophet::Success);
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
             next_hop_,local_instance_);
}

void
ProphetEncounter::handle_poll_timeout()
{
    log_debug("handle_poll_timeout");

    deadcount_++;
    prophet_state_t state = get_state("handle_poll_timeout");
    switch(state) {
        case SYNSENT:
        case WAIT_NB:
            if (synsender_ == true)
            {
                enqueue_hello(Prophet::SYN,
                              Prophet::UniqueID::tid(),
                              Prophet::NoSuccessAck);
            }
            break;
        case SYNRCVD:
            enqueue_hello(Prophet::SYNACK,
                          tid_,
                          Prophet::Success);
            break;
        case ESTAB:
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
            enqueue_hello(Prophet::ACK,
                          tid_,
                          Prophet::Success);
            break;
        case WAIT_RIB:
            enqueue_hello(Prophet::ACK,
                          tid_,
                          Prophet::Success);
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
            }
            break;
        case OFFER:
            send_bundle_offer();
            break;
        case WAIT_INFO:
            // On the first phase of INFO_EXCHANGE,
            // synsender_ is initiator and !synsender_ is
            // listener.  During this phase, immediately
            // switch roles and continue.
            if ((synsender_ && initiator_) ||
                (!synsender_ && !initiator_))
            {
                switch_info_role();
            }
            // otherwise, wait for 1/2 hello_dead before
            // switching phases
            else
            {
                struct timeval now;
                ::gettimeofday(&now,0);
                u_int diff = TIMEVAL_DIFF_MSEC(now,data_sent_);
                u_int timeout = oracle_->params()->hello_dead_ * timeout_ / 2;
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
    buf->appendf("%05d  %10s  %s\n",remote_instance_,
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

    bool update_dictionary = false;

    // Grab a list of Bundles from queueing policy enforcer
    BundleVector vec;
    oracle_->bundles()->vector(vec);

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
    ProphetBundleOffer offer(vec,fs,d);
    if (offers_.empty())
        offers_.set_type(BundleOffer::OFFER);
    else
    {
        BundleOffer* bo = offers_.front();
        ASSERT(bo->type() == offers_.type());
    }

    while(!offer.empty())
    {
        Bundle* b = offer.top();
        offer.pop();

        EndpointID eid = Prophet::eid_to_routeid(b->dest_);
        u_int16_t sid = ribd_.find(eid);

        // either not found or initiator's EID
        if (sid == 0) 
        {
            if (ribd_.is_assigned(eid) == true &&
                synsender_ == true)
            {
                // local destination, don't offer
                continue;
            }

            sid = ribd_.insert(eid);
            update_dictionary = true;
        }
        else
        if (sid == 1 && synsender_ == false)
        {
            // local destination, don't offer
            continue;
        }

        ASSERT(offers_.type() == BundleOffer::OFFER);

        // add bundle listing to the offer
        offers_.add_offer(b,sid);
    }

    // Now append all known ACKs
    PointerList<ProphetAck> acklist;
    oracle_->acks()->fetch(EndpointIDPattern("dtn://*"),acklist);

    for (PointerList<ProphetAck>::iterator i = acklist.begin();
         i != acklist.end();
         i++)
    {
        ProphetAck* pa = *i;

        EndpointID eid = Prophet::eid_to_route(pa->dest_id_);
        u_int16_t sid = ribd_.find(eid);

        if (sid == 0)
        {
            if (ribd_.is_assigned(eid) == false)
            {
                sid = ribd_.insert(eid);
            }

            if (sid != 0) 
            {
                update_dictionary = true;
            }
        }
        
        // add ACK to the list
        offers_.add_offer(pa->cts_,sid,false,false,true);
    }

    if (update_dictionary)
        send_dictionary();

    enqueue_bundle_tlv(offers_,
                       Prophet::UniqueID::tid(),
                       Prophet::NoSuccessAck);
}

void
ProphetEncounter::send_dictionary()
{
    log_debug("send_dictionary");
    ASSERT(oracle_ != NULL);
    ASSERT(oracle_->nodes() != NULL);
    ASSERT(next_hop_ != NULL);

    // list of all known ProphetNodes, indexed by peer endpoint id
    ProphetNodeList list;
    // list of predictability values for each node, indexed by string id
    RIBTLV::List rib;
    // EIDs for peer endpoints in this exchange
    EndpointID local(BundleDaemon::instance()->local_eid()),
               remote(next_hop_->remote_eid());

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
    oracle_->nodes()->dump_table(list);
    
    // walk over the master node list and create SIDs for each
    for(ProphetNodeList::iterator i = list.begin();
        i != list.end();
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

        ASSERT(sid != 0);
        rib.push_back(new RIBNode(node,sid));
    }

    // ask ProphetController for list of active bundles
    BundleVector vec;
    oracle_->bundles()->vector(vec);
    for (BundleVector::iterator i = vec.begin();
         i != vec.end();
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

    enqueue_ribd(ribd_,
                 Prophet::UniqueID::tid(),
                 Prophet::NoSuccessAck);
    enqueue_rib(rib);
}

bool
ProphetEncounter::send_prophet_tlv()
{
    oasys::ScopeLock l(otlv_lock_,"send_prophet_tlv");
    ASSERT(outbound_tlv_.size() != 0);

    if (neighbor_gone_ == true) return false;

    // establish some flow control
    struct timeval now;
    ::gettimeofday(&now,0);
    if ( TIMEVAL_DIFF_MSEC(now,data_sent_) <= (timeout_ / 2)) 
    {
        log_debug("flow control: timediff %u timeout_ %u",
                (u_int)TIMEVAL_DIFF_MSEC(now,data_sent_), timeout_);
        return false;
    }

    ProphetTLV* pt;

    while ((pt = outbound_tlv_.front()) != NULL)
    {
        // encapsulate ProphetTLV into Bundle and queue up
        BundleRef b("ProphetEncounter send_prophet_tlv");
        b = new Bundle();
        if (pt->create_bundle(b.object(),
                              BundleDaemon::instance()->local_eid(),
                              next_hop_->remote_eid()))
        {
            // enqueue before non-control bundles
            fwd_to_nexthop(b.object(),true);
        }
        else
        {
            log_err("Failed to write out ProphetTLV");
            return false;
        }

        // clear out what we've sent
        outbound_tlv_.pop_front();
        delete pt;
    } 

    return true;
}

ProphetTLV*
ProphetEncounter::outbound_tlv(u_int32_t tid,
                               Prophet::header_result_t result,
                               Prophet::prophet_tlv_t type)
{
    oasys::ScopeLock l(otlv_lock_,"outbound_tlv");
    ProphetTLV* tlv = NULL;

    // first check if similar TLV type already enqueued
    for(TLVList::iterator i = outbound_tlv_.begin();
        i != outbound_tlv_.end();
        i++)
    {
        tlv = *i;
        ProphetTLV::List list = tlv->list();
        for(ProphetTLV::List::const_iterator j =
                (ProphetTLV::List::const_iterator) list.begin();
            j != list.end();
            j++)
        {
            BaseTLV* bt = *j;
            if (bt->typecode() == type)
                return NULL;
        }
    }

    // tlv now points to last TLV in queue
    // and no other tlv is of same type
    if (tlv != NULL)
    {
        // test whether tid and result are compatible
        // if no match, create new tlv below
        if (((tid != 0) &&
             (tlv->transaction_id() != tid)) ||
            (tlv->result() != result))
        {
            tlv = NULL;
        }
        else
            // shrink the list by one,
            // caller is expected to add it back in
            outbound_tlv_.pop_back();
    }

    // create new TLV if no match above
    if (tlv == NULL)
    {
        tlv = new ProphetTLV(result,
                             local_instance_,
                             remote_instance_,
                             tid);
    }

    return tlv;
}

void
ProphetEncounter::enqueue_hello(Prophet::hello_hf_t hf,
                                u_int32_t tid,
                                Prophet::header_result_t result)
{
    oasys::ScopeLock l(otlv_lock_,"enqueue_hello");
    log_debug("enqueue_hello %s %d %s",
               Prophet::hf_to_str(hf), tid,
               Prophet::result_to_str(result));

    // look for matching header already in queue
    ProphetTLV* tlv = outbound_tlv(tid,result,Prophet::HELLO_TLV);

    if (tlv == NULL )
    {
        log_debug("duplicate hello message already queued");
        return;
    }

    tlv->add_tlv(
        new HelloTLV(hf,
                     oracle_->params()->hello_interval_,
                     BundleDaemon::instance()->local_eid(),
                     logpath_)
    );
    outbound_tlv_.push_back(tlv);
}

void
ProphetEncounter::enqueue_ribd(const ProphetDictionary& ribd,
                               u_int32_t tid,
                               Prophet::header_result_t result)
{
    oasys::ScopeLock l(otlv_lock_,"enqueue_ribd");
    log_debug("enqueue_ribd (%zu entries) %u %s",
               ribd.size(),tid,
               Prophet::result_to_str(result));

    // look for matching header already in queue
    ProphetTLV* tlv = outbound_tlv(tid,result,Prophet::RIBD_TLV);

    if (tlv == NULL)
    {
        log_debug("duplicate dictionary already queued");
        return;
    }

    tlv->add_tlv(new RIBDTLV(ribd,logpath_));
    outbound_tlv_.push_back(tlv);
}

void
ProphetEncounter::enqueue_rib(const RIBTLV::List& nodes,
                              u_int32_t tid,
                              Prophet::header_result_t result)
{
    oasys::ScopeLock l(otlv_lock_,"enqueue_rib");
    log_debug("enqueue_rib (%zu entries)",nodes.size());

    // look for matching header already in queue
    ProphetTLV* tlv = outbound_tlv(tid,result,Prophet::RIB_TLV);

    if (tlv == NULL)
    {
        log_debug("duplicate RIB already queued");
        return;
    }

    tlv->add_tlv(
        new RIBTLV(nodes,
                   oracle_->params()->relay_node_,
                   oracle_->params()->custody_node_,
                   oracle_->params()->internet_gw_,
                   logpath_)
    );
    outbound_tlv_.push_back(tlv);
}

void
ProphetEncounter::enqueue_bundle_tlv(const BundleOfferList& bundles,
                                     u_int32_t tid,
                                     Prophet::header_result_t result)
{
    oasys::ScopeLock l(otlv_lock_,"enqueue_bundle_tlv");
    log_debug("enqueue_bundle_tlv (%zu entries)",bundles.size());

    // look for matching header already in queue
    ProphetTLV* tlv = outbound_tlv(tid,result,Prophet::BUNDLE_TLV);

    if (tlv == NULL)
    {
        log_debug("duplicate Bundle TLV already queued");
        return;
    }

    tlv->add_tlv(new BundleTLV(bundles,logpath_));
    outbound_tlv_.push_back(tlv);
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
    ribd_.clear();

    if (synsender_ == true)
    { 
        enqueue_hello(Prophet::SYN,
                      Prophet::UniqueID::tid(),
                      Prophet::NoSuccessAck);
        set_state(SYNSENT);
        ASSERT(ribd_.assign(local,0));
        ASSERT(ribd_.assign(remote,1));
    }
    else
    {
        ASSERT(ribd_.assign(local,1));
        ASSERT(ribd_.assign(remote,0));
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
    deadcount_ = 0;
    while (neighbor_gone_ == false) {

        if (cmdqueue_.size() != 0)
        {
            process_command();
            continue;
        }

        if ((outbound_tlv_.size() != 0) &&
            send_prophet_tlv())
        {
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
                                        POLL_IN,&revents,timeout);

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

        if (deadcount_ >= oracle_->params()->hello_dead_) {
            log_err("%d silent Hello intervals, giving up",deadcount_);
            break;
        }
    }

    ProphetController::instance()->unreg(this);
}

} // namespace dtn

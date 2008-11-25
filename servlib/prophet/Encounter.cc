/*
 *    Copyright 2007 Baylor University
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

#include "BundleCore.h"
#include "HelloTLV.h"
#include "RIBDTLV.h"
#include "RIBTLV.h"
#include "OfferTLV.h"
#include "ResponseTLV.h"
#include "TLVCreator.h"
#include "Encounter.h"

#define NEXT_TID (++next_tid_ == 0) ? ++next_tid_ : next_tid_

#define PROPHET_TLV(_tlv, _result, _tid) do { \
    _tlv = new ProphetTLV(                    \
            oracle_->core()->prophet_id(),    \
            oracle_->core()->prophet_id(next_hop_), \
            _result,                          \
            local_instance_,                  \
            remote_instance_,                 \
            (_tid == 0) ? NEXT_TID : _tid); \
    } while (0)

#define SEND_ACK(_tid) send_hello(HelloTLV::ACK, \
                                  ProphetTLV::NoSuccessAck,_tid)
#define SEND_SYN(_tid) send_hello(HelloTLV::SYN, \
                                  ProphetTLV::NoSuccessAck,_tid)
#define SEND_SYNACK(_tid) send_hello(HelloTLV::SYNACK, \
                                     ProphetTLV::NoSuccessAck,_tid)
#define SEND_RSTACK(_tid) send_hello(HelloTLV::RSTACK,\
                                     ProphetTLV::Failure,_tid)

#define LOG(_level, _args...) oracle_->core()->print_log( \
        name_.c_str(), BundleCore::_level,  _args )

#define SET_STATE(_new) do { LOG(LOG_DEBUG, "state_ %s -> %s %s:%d", \
        state_to_str(state_), state_to_str(_new), __FILE__, __LINE__); \
        state_ = _new; \
    } while (0)

#define UPDATE_PEER_VERIFIER(_sender_instance) do {     \
        remote_instance_ = _sender_instance;            \
        LOG(LOG_DEBUG, "update peer verifier %d",       \
                       (_sender_instance)); } while (0) \

#define ASSIGN_ROLES(_s,_r) do { \
        if (synsender_) { \
            _s = oracle_->core()->local_eid(); \
            _r = next_hop_->remote_eid(); } \
        else { \
            _s = next_hop_->remote_eid(); \
            _r = oracle_->core()->local_eid(); }\
    } while (0) 


namespace prophet
{

Encounter::Encounter(const Link* nexthop,
                     Oracle* oracle,
                     u_int16_t instance)
    : ExpirationHandler(),
      oracle_(oracle),
      local_instance_(instance),
      remote_instance_(0),
      tid_(0),
      next_tid_(0),
      timeout_(oracle_->params()->hello_interval() * 100),
      next_hop_(nexthop),
      tlv_(NULL),
      synsender_(oracle_->core()->local_eid().compare(
                  next_hop_->remote_eid()) < 0),
      state_(WAIT_NB),
      synsent_(false),
      estab_(false),
      neighbor_gone_(false),
      remote_nodes_(oracle_->core(),"remote",false),
      hello_rate_(0),
      data_sent_(time(0)),
      data_rcvd_(time(0)),
      alarm_(NULL)
{
    std::string name("encounter-");
    char buff[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    size_t len = snprintf(buff,10,"%d",instance);
    name.append(buff,len);
    ExpirationHandler::set_name(name.c_str());

    if (synsender_)
        if (send_hello(HelloTLV::SYN))
            SET_STATE(SYNSENT);

    // set up reminder for timeout_ milliseconds
    alarm_ = oracle_->core()->create_alarm(this,timeout_);
    if (alarm_ == NULL) neighbor_gone_ = true;

    LOG(LOG_DEBUG,"constructor(%s,%u)",nexthop->remote_eid(),instance);

}

Encounter::Encounter(const Encounter& e)
    : ExpirationHandler(e),
      oracle_(e.oracle_),
      local_instance_(e.local_instance_),
      remote_instance_(e.remote_instance_),
      tid_(e.tid_),
      timeout_(e.timeout_),
      next_hop_(e.next_hop_),
      tlv_(e.tlv_),
      synsender_(e.synsender_),
      state_(e.state_),
      synsent_(e.synsent_),
      estab_(e.estab_),
      neighbor_gone_(e.neighbor_gone_),
      local_ribd_(e.local_ribd_),
      remote_ribd_(e.remote_ribd_),
      remote_offers_(e.remote_offers_),
      local_response_(e.local_response_),
      remote_nodes_(e.remote_nodes_),
      hello_rate_(e.hello_rate_),
      data_sent_(e.data_sent_),
      data_rcvd_(e.data_rcvd_),
      alarm_(NULL)
{
    LOG(LOG_DEBUG,"copy constructor(%u)",local_instance_);
    alarm_ = oracle_->core()->create_alarm(this,e.alarm_->time_remaining());
    if (alarm_ == NULL) neighbor_gone_ = true;
}

Encounter::~Encounter()
{
    if (alarm_ != NULL && alarm_->pending())
        alarm_->cancel();
}

void
Encounter::hello_interval_changed()
{
    if (neighbor_gone_) return;

    // nothing to do if it's the same value
    u_int timeout = oracle_->params()->hello_interval() * 100;
    if (timeout == timeout_) return;

    // nothing to do for alarm_ unless it is pending
    if (alarm_->pending()) 
    {
        if (alarm_->time_remaining() == 0)
            // no change, let timeout handler catch it
            goto set_timeout;

        // grab the difference from old to new
        u_int diff;
        u_int new_alarm;
        if (timeout_ > timeout) 
        {
            // new timeout is shorter than old
            diff = timeout_ - timeout;
            new_alarm = alarm_->time_remaining() < diff ? 
                        0 : alarm_->time_remaining() - diff;
        }
        else
        {
            // new timeout is longer than old
            diff = timeout - timeout_;
            new_alarm = alarm_->time_remaining() + diff;
        }

        // take the old timeout value off the schedule
        alarm_->cancel();

        // write new timeout value to alarm
        alarm_ = oracle_->core()->create_alarm(this,new_alarm);

        // give up on error
        if (alarm_ == NULL) neighbor_gone_ = true;
    }

set_timeout:
    LOG(LOG_DEBUG,"hello_interval changed (%u -> %u)", timeout_,timeout);

    // store the new timeout
    timeout_ = timeout;
}

bool
Encounter::receive_tlv(ProphetTLV* tlv)
{
    if (neighbor_gone_) return false;

    if (tlv == NULL)
        return false;

    LOG(LOG_DEBUG,"receive_tlv");

    // update member pointer
    tlv_ = tlv;

    // update timestamp
    data_rcvd_ = time(0);

    // disarm
    if (alarm_->pending())
        alarm_->cancel();

    // capture transaction id
    tid_ = tlv_->transaction_id();

    BaseTLV* bt = NULL; 
    bool ok = true;
    // distribute the individual TLVs to the correct handler
    while ( (bt = tlv_->get_tlv()) != NULL && ok )
    {
        ok = dispatch_tlv(bt);
        delete bt;
    }

    // clean up memory
    delete tlv_;
    tlv_ = NULL;

    if (ok)
    {
        // clean up old alarm: host implementation cleans up cancelled
        // alarms, else we clean up our own spent alarms
        if (alarm_->pending())
            alarm_->cancel();
        else
            delete alarm_;
        // reschedule the timeout handler
        alarm_ = oracle_->core()->create_alarm(this,timeout_,true);

        // give up on error
        if (alarm_ == NULL) neighbor_gone_ = true;
    } 
    else
        // fail out on error
        neighbor_gone_ = true;

    return ok;
}

bool
Encounter::dispatch_tlv(BaseTLV* tlv)
{
    LOG(LOG_DEBUG,"dispatch_tlv");

    bool ok = false;
    bool pre_dispatch = estab_;

    switch (tlv->typecode())
    {
        case BaseTLV::HELLO_TLV:
            ok = handle_hello_tlv(tlv);
            if (ok && !pre_dispatch && estab_)
            {
                // Hello procedure just completed ... move from ESTAB to 
                // either CREATE_DR (synsender_) or WAIT_DICT (!synsender_)
                if (synsender_)
                {
                    SET_STATE(CREATE_DR);
                    bool ok = send_dictionary_rib();
                    if (ok) SET_STATE(SEND_DR);
                    return ok;
                }
                else
                    SET_STATE(WAIT_DICT);
            }
            return ok;
        case BaseTLV::RIBD_TLV:
            if (! estab_) break;
            return handle_ribd_tlv(tlv);
        case BaseTLV::RIB_TLV:
            if (! estab_) break;
            return handle_rib_tlv(tlv);
        case BaseTLV::OFFER_TLV:
            if (! estab_) break;
            ok = handle_offer_tlv(tlv);
            if (ok && synsender_ && state_ == WAIT_INFO)
            {
                // finished Initiator phase, now switch to Listener
                SET_STATE(WAIT_DICT);
            }
            return ok;
        case BaseTLV::RESPONSE_TLV:
            if (! estab_) break;
            ok = handle_response_tlv(tlv);
            if (ok && !synsender_ && state_ == WAIT_INFO)
            {
                // finished Listener phase, now switch to Initiator
                SET_STATE(CREATE_DR);
                ok = send_dictionary_rib();
                if (ok) SET_STATE(SEND_DR);
            }
            return ok;
        case BaseTLV::ERROR_TLV:
        case BaseTLV::UNKNOWN_TLV:
        default:
            return false;
    }

    // not reached unless !estab && tlv->typecode() != HELLO_TLV

    hello_rate_ = 2;
    if (state_ == SYNSENT)
        return SEND_SYN(NEXT_TID);
    else if (state_ == SYNRCVD)
        return SEND_SYNACK(tid_);

    // unless ERROR or UNKNOWN, do not fail out of this peering session
    return true;
}

void
Encounter::handle_bundle_received(const Bundle* b)
{
    if (state_ != REQUEST) return;
    if (b == NULL) return;

    LOG(LOG_DEBUG,"handle_bundle_received: "
            "%s %u:%u",b->destination_id().c_str(),
            b->creation_ts(), b->sequence_num());

    // reduce dest id to node id
    std::string eid = oracle_->core()->get_route(b->destination_id());

    // translate node id to sid
    u_int sid = local_ribd_.find(eid);

    // check for dictionary error
    if (sid == Dictionary::INVALID_SID) return;

    // reduce list of responses by this one
    local_response_.remove_entry(
            b->creation_ts(),
            b->sequence_num(),
            sid);

    // clean up previous alarm: host implementation cleans up cancelled
    // alarms, else we clean up our own spent alarms
    if (alarm_->pending())
        alarm_->cancel();
    else
        delete alarm_;

    // set up reminder for timeout_ milliseconds
    alarm_ = oracle_->core()->create_alarm(this,timeout_,true);

    // give up on error
    if (alarm_ == NULL) neighbor_gone_ = true;
}

void
Encounter::handle_timeout()
{
    LOG(LOG_DEBUG,"handle_timeout");

    if (neighbor_gone_) return;

    bool ok = false;
    switch (state_)
    {
        case WAIT_NB:
            ok = true;
            SET_STATE(WAIT_NB);
            break;
        case SYNSENT:
            ok = SEND_SYN(NEXT_TID);
            SET_STATE(SYNSENT);
            break;
        case SYNRCVD:
            ok = SEND_SYNACK(tid_);
            SET_STATE(SYNRCVD);
            break;
        case ESTAB:
            // should not be reached
            ok = false;
            break;
        case WAIT_DICT:
            ok = SEND_ACK(tid_);
            SET_STATE(WAIT_DICT);
            break;
        case WAIT_RIB:
            ok = SEND_ACK(tid_);
            SET_STATE(WAIT_DICT);
            break;
        case OFFER:
            ok = send_offer();
            SET_STATE(OFFER);
            break;
        case SEND_DR:
            ok = send_dictionary_rib();
            if (ok) SET_STATE(SEND_DR);
            break;
        case REQUEST:
            ok = send_response();
            break;
        case WAIT_INFO:
            // if time_diff(now,data_sent) > hello_dead/2 
            // start InfoExch again
            if (time(0) - data_rcvd_ >
                (oracle_->params()->hello_dead() *
                 oracle_->params()->hello_interval() / 20))
            {
                if (synsender_)
                {
                    SET_STATE(CREATE_DR);
                    ok = send_dictionary_rib();
                    if (ok) SET_STATE(SEND_DR);
                }
                else
                {
                    SET_STATE(WAIT_DICT);
                    ok = true;
                }
            }
            else
                ok = true;
            break;
        default:
            break;
    }
    // if error sending message, or if silence exceeds max, then
    // signal session death and fail out
    if (!ok ||
        (time(0) - data_rcvd_) >
        (oracle_->params()->hello_dead() *
                            /* convert 100's of ms to seconds */
         oracle_->params()->hello_interval() / 10))
    {
        neighbor_gone_ = true;
        return;
    }

    // clean up previous alarm: host implementation cleans up cancelled
    // alarms, else we clean up our own spent alarms
    if (alarm_->pending())
        alarm_->cancel();
    else
        delete alarm_;

    // set up reminder for timeout_ milliseconds
    alarm_ = oracle_->core()->create_alarm(this,timeout_,true);

    // give up on error
    if (alarm_ == NULL) neighbor_gone_ = true;
}

bool
Encounter::handle_hello_tlv(BaseTLV* tlv)
{
    if (tlv_ == NULL) return false;
    HelloTLV* hello = static_cast<HelloTLV*>(tlv);
    if (hello == NULL || hello->typecode() != BaseTLV::HELLO_TLV)
        return false;

    LOG(LOG_DEBUG,"handle_hello_tlv(%s,%u) from %s",
            hello->hf_str(),hello->timer(),hello->sender().c_str());

    // build out truth table as laid out in Section 5.2
    bool hello_a = (remote_instance_ == tlv_->sender_instance());
    LOG(LOG_DEBUG,"hello_a %s remote_instance_ %u tlv %u",
            hello_a ? "true" : "false", remote_instance_,
            tlv_->sender_instance());
    bool hello_b = hello_a &&
        (oracle_->core()->prophet_id(next_hop_) == tlv_->source());
    LOG(LOG_DEBUG,"hello_b %s next_hop_ %s tlv %s",
            hello_b ? "true" : "false", 
            oracle_->core()->prophet_id(next_hop_).c_str(),
            tlv_->source().c_str());
    bool hello_c = (local_instance_ == tlv_->receiver_instance());
    LOG(LOG_DEBUG,"hello_c %s local_instance_ %u tlv %u",
            hello_c ? "true" : "false",
            local_instance_, tlv_->receiver_instance());

    // negotiate a common timeout by choosing minimum
    u_int timeout = std::min((u_int)oracle_->params()->hello_interval(),
                             (u_int)hello->timer(), std::less<u_int>());

    if (!estab_ && (timeout * 100) != timeout_)
    {
        LOG(LOG_DEBUG,"timeout_ %u -> %u (line %d)",
                timeout_, timeout, __LINE__);
        // convert to milliseconds
        timeout_ = timeout * 100;
    }

    bool ok = true;
    switch (hello->hf())
    {
    case HelloTLV::SYN:
        if (estab_)
        {
            // note 2, 5.2.1
            hello_rate_ = 2;
            ok = SEND_ACK(tid_);
        }
        else
        if (state_ == SYNSENT ||
            state_ == SYNRCVD ||
            state_ == WAIT_NB)
        {
            UPDATE_PEER_VERIFIER(tlv_->sender_instance());
            LOG(LOG_DEBUG,"handle_hello_tlv(SYN): state_ %s remote_instance_ %u",
                    state_to_str(state_),remote_instance_);
            ok = SEND_SYNACK(tid_);
            SET_STATE(SYNRCVD);
        }
        break;
    case HelloTLV::SYNACK:
        if (estab_)
        {
            // note 2, 5.2.1
            hello_rate_ = 2;
            ok = SEND_ACK(tid_);
        }
        else
        if (state_ == SYNSENT)
        {
            if ( hello_c)
            {
                hello_rate_ = 0;
                UPDATE_PEER_VERIFIER(tlv_->sender_instance());
                if ((ok = SEND_ACK(tid_)) != 0)
                {
                    SET_STATE(ESTAB);
                    estab_ = true;
                }
            }
            else
            {
                ok = SEND_RSTACK(tid_);
                SET_STATE(SYNSENT);
            }
        }
        else
        if (state_ == SYNRCVD)
        {
            if (hello_c)
            {
                hello_rate_ = 0;
                UPDATE_PEER_VERIFIER(tlv_->sender_instance());
                if ((ok = SEND_ACK(tid_)) != 0)
                {
                    SET_STATE(ESTAB);
                    estab_ = true;
                }
            }
            else
            {
                ok = SEND_RSTACK(tid_);
                SET_STATE(SYNRCVD);
            }
        }
        break;
    case HelloTLV::ACK:
        if (estab_ && !(hello_b && hello_c))
        {
            ok = SEND_RSTACK(tid_);
            break;
        }
        if (state_ == SYNSENT)
        {
            ok = SEND_RSTACK(tid_);
            SET_STATE(SYNSENT);
        }
        else
        if (state_ == SYNRCVD)
        {
            if (hello_b && hello_c)
            {
                SET_STATE(ESTAB);
                estab_ = true;
            }
            else
            {
                LOG(LOG_DEBUG,"handle_hello_tlv(ACK): state_ SYNRCVD "
                        "remote_instance_ %u tlv instance %u",
                        remote_instance_,tlv_->sender_instance());
                ok = SEND_RSTACK(tid_);
                SET_STATE(SYNRCVD);
            }
        }
        else
        if (state_ == WAIT_RIB || state_ == OFFER)
        {
            SET_STATE(WAIT_DICT);
        }
        else
        if (state_ == REQUEST)
        {
            SET_STATE(CREATE_DR);
            ok = send_dictionary_rib();
            if (ok) SET_STATE(SEND_DR);
        }
        else
        if (state_ == WAIT_INFO)
        {
            // remote says "What up?" so we wake up and move on
            if (time(0) - data_sent_ > oracle_->params()->hello_interval())
            {
                if (synsender_)
                {
                    SET_STATE(CREATE_DR);
                    bool ok = send_dictionary_rib();
                    if (ok) SET_STATE(SEND_DR);
                    return ok;
                }
                else
                {
                    SET_STATE(WAIT_DICT);
                }
            }
        }
        break;
    case HelloTLV::RSTACK:
        if (hello_a && hello_c && !synsent_)
        {
            // signal end of session
            return false;
        }
        break;
    case HelloTLV::HF_UNKNOWN:
    default:
        break;
    }

    return ok;
}

bool
Encounter::handle_ribd_tlv(BaseTLV* tlv)
{
    LOG(LOG_DEBUG,"handle_ribd_tlv");

    RIBDTLV* ribd = static_cast<RIBDTLV*>(tlv);
    if (ribd == NULL)
        return false;

    // figure out which one of us is sender and which is receiver
    std::string sender, receiver;
    ASSIGN_ROLES(sender,receiver);

    switch (state_)
    {
    case WAIT_INFO:
        // remote says "What up?" so we wake up and move on
        if (time(0) - data_sent_ > oracle_->params()->hello_interval())
        {
            if (synsender_)
                return false;
            else
                SET_STATE(WAIT_DICT);
        }
    case WAIT_DICT:
    case WAIT_RIB:
        remote_ribd_ = ribd->ribd(sender,receiver);
        remote_ribd_.dump(oracle_->core(),__FILE__,__LINE__);
        SET_STATE(WAIT_RIB);
        return true;
    case OFFER:
        SET_STATE(OFFER);
        return send_offer();
    default:
        break;
    }
    return false;
}

bool
Encounter::handle_rib_tlv(BaseTLV* tlv)
{
    LOG(LOG_DEBUG,"handle_rib_tlv");

    RIBTLV* rib = static_cast<RIBTLV*>(tlv);
    if (rib == NULL)
        return false;

    remote_ribd_.dump(oracle_->core(),__FILE__,__LINE__);
    Table* nodes = oracle_->nodes();
    if (state_ == WAIT_RIB)
    {
        // update p for remote peer
        nodes->update_route(next_hop_->remote_eid(),
                            rib->relay(),
                            rib->custody(),
                            rib->internet());
        // then update p for each RIB entry
        nodes->update_transitive(next_hop_->remote_eid(),
                                 rib->nodes(),
                                 remote_ribd_);
        // keep a local copy of remote's RIB
        remote_nodes_.assign(rib->nodes(),
                             remote_ribd_);
        SET_STATE(OFFER);
        return send_offer();
    } 
    return false;
}

bool
Encounter::handle_offer_tlv(BaseTLV* tlv)
{
    LOG(LOG_DEBUG,"handle_offer_tlv");

    OfferTLV* offer = static_cast<OfferTLV*>(tlv);
    if (offer == NULL)
    {
        LOG(LOG_DEBUG,"failed to downcast tlv");
        return false;
    }

    if (state_ == SEND_DR || state_ == REQUEST)
    {
        remote_offers_ = offer->list();
        LOG(LOG_DEBUG,"received %zu offers",remote_offers_.size()); 
        SET_STATE(REQUEST);
        return send_response();
    }
    else if (state_ == WAIT_INFO)
    {
        // ignore
        return true;
    }
    else
    {
        LOG(LOG_ERR,"received offer tlv when state_ == %s",state_str());
    }

    return false;
}

bool
Encounter::handle_response_tlv(BaseTLV* tlv)
{
    LOG(LOG_DEBUG,"handle_response_tlv");

    ResponseTLV* response = static_cast<ResponseTLV*>(tlv);
    if (response == NULL)
        return false;

    switch (state_)
    {
    case WAIT_RIB:
        SET_STATE(WAIT_DICT);
        return SEND_ACK(tid_);
    case OFFER:
        // empty request signals state change
        if (response->list().empty())
        {
            LOG(LOG_DEBUG,"received empty request");
            SET_STATE(WAIT_INFO);
            return true;
        }
        else
        {
            LOG(LOG_DEBUG,"received %zu requests",response->list().size());
            // enumerate bundle list from repository
            BundleList bundles = oracle_->core()->bundles();
            // dump the dictionary 
            remote_ribd_.dump(oracle_->core(),__FILE__,__LINE__);

            // walk the list of requests
            for(BundleResponseList::const_iterator i =
                    response->list().begin();
                i != response->list().end();
                i++)
            {
                // look up the requested bundle
                std::string eid = remote_ribd_.find((*i)->sid());
                const Bundle* b = oracle_->core()->find(bundles,
                        eid,(*i)->creation_ts(),(*i)->seqno());

                // ignore failure (after logging)
                if (b == NULL) 
                {
                    LOG(LOG_ERR,"failed to locate bundle for request "
                        "%u -> %s, %u, %u", (*i)->sid(),
                        remote_ribd_.find((*i)->sid()).c_str(),
                        (*i)->creation_ts(),(*i)->seqno());
                    continue;
                }

                // skip if send fails, try again next timeout
                if (!oracle_->core()->send_bundle(b,next_hop_))
                {
                    LOG(LOG_ERR,"failed to send bundle for request "
                        "%s, %u, %u", remote_ribd_.find((*i)->sid()).c_str(),
                        (*i)->creation_ts(),(*i)->seqno());
                    continue;
                }

                // update timestamp on outbound data
                data_sent_ = time(0);

            }
            SET_STATE(OFFER);
            return true;
        }
    default:
        LOG(LOG_ERR,"received response when state_ == %s",
                state_to_str(state_));
        break;
    }

    return false;
}

bool
Encounter::send_hello(HelloTLV::hello_hf_t hf,
                      ProphetTLV::header_result_t hr,
                      u_int32_t tid)
{
    // Impose a simple flow control; hello_rate_ is set by the constructor
    // and by handle_hello_tlv
    if ((state_ != WAIT_NB) &&
            (hello_rate_ > 0) &&
            (hf != HelloTLV::RSTACK) &&
            (time(0) - data_sent_ < timeout_ / hello_rate_) )
        return true; // don't kill the Encounter, but neither do we
                     // send this Hello message

    // Create Hello TLV with hello function hf
    HelloTLV* ht = new HelloTLV(hf,
                                oracle_->params()->hello_interval(),
                                oracle_->core()->local_eid());

    LOG(LOG_DEBUG,"send_hello(%s,%u)",
            ht->hf_str(),ht->timer());

    // Create outbound TLV
    ProphetTLV* tlv = NULL;
    PROPHET_TLV(tlv,hr,tid);
    if (tlv == NULL)
    {
        delete ht;
        delete tlv;
        return false;
    }

    // Attach the Hello TLV
    if (!tlv->add_tlv(ht))
    {
        delete ht;
        delete tlv;
        return false;
    }

    // Submit TLV as a bundle to the host bundle core
    bool ok = send_tlv(tlv);

    // set flag on SYN sent 
    if (ok && (hf == HelloTLV::SYN || hf == HelloTLV::SYNACK))
        synsent_ = true;

    return ok;
}

bool
Encounter::send_dictionary_rib(ProphetTLV::header_result_t hr,
                               u_int32_t tid)
{
    LOG(LOG_DEBUG,"send_dictionary_rib");

    if (state_ != CREATE_DR && state_ != SEND_DR)
        return false;

    // figure out which one of us is sender and which is receiver
    std::string sender, receiver;
    ASSIGN_ROLES(sender,receiver);

    // Create the dictionary TLV
    RIBDTLV* ribdtlv = TLVCreator::ribd(oracle_->core(),
                                        oracle_->nodes(),
                                        sender, receiver);

    if (ribdtlv == NULL) return false;

    // hold on to that dictionary, for creating RIB and Offer
    local_ribd_ = ribdtlv->ribd(sender,receiver);
    local_ribd_.dump(oracle_->core(),__FILE__,__LINE__);

    // Create outbound TLV
    ProphetTLV* tlv = NULL;
    PROPHET_TLV(tlv,hr,tid);
    if (tlv == NULL)
    {
        delete ribdtlv;
        return false;
    }

    // Attach the RIBD TLV
    if (!tlv->add_tlv(ribdtlv))
    {
        delete ribdtlv;
        delete tlv;
        return false;
    }

    RIBTLV* rib = TLVCreator::rib(oracle_,
                                  local_ribd_,
                                  oracle_->params()->relay_node(),
                                  oracle_->core()->custody_accepted(),
                                  oracle_->params()->internet_gw());

    if (rib == NULL)
    {
        delete tlv;
        return false;
    }

    // Attach the RIB TLV
    if (!tlv->add_tlv(rib))
    {
        delete rib;
        delete tlv;
        return false;
    }

    // Submit TLV as a bundle to the host bundle core
    return send_tlv(tlv);
}

bool
Encounter::send_offer(ProphetTLV::header_result_t hr,
                      u_int32_t tid)
{
    LOG(LOG_DEBUG,"send_offer");
    remote_ribd_.dump(oracle_->core(),__FILE__,__LINE__);

    // remote nodes is filled in by handle_rib_tlv
    OfferTLV* offer = TLVCreator::offer(oracle_,
                                        next_hop_,
                                        remote_ribd_,
                                        remote_nodes_);

    if (offer == NULL) return false;

    // Create outbound TLV
    ProphetTLV* tlv = NULL;
    PROPHET_TLV(tlv,hr,tid);
    if (tlv == NULL)
    {
        delete offer;
        return false;
    }

    // Attach the RIBD TLV
    if (!tlv->add_tlv(offer))
    {
        delete offer;
        delete tlv;
        return false;
    }

    // Submit TLV as a bundle to the host bundle core
    return send_tlv(tlv);
}

bool
Encounter::send_response(ProphetTLV::header_result_t hr,
                         u_int32_t tid)
{
    LOG(LOG_DEBUG,"send_response");
    local_ribd_.dump(oracle_->core(),__FILE__,__LINE__);

    ResponseTLV* response = TLVCreator::response(oracle_,
                                                 remote_offers_,
                                                 local_response_,
                                                 local_ribd_);

    if (response == NULL) return false;

    // Create outbound TLV
    ProphetTLV* tlv = NULL;
    PROPHET_TLV(tlv,hr,tid);
    if (tlv == NULL) 
    {
        delete response;
        return false;
    }

    // Attach the RIBD TLV
    if (!tlv->add_tlv(response)) 
    {
        delete response;
        delete tlv;
        return false;
    }

    // empty response means state change
    bool wait_info = local_response_.empty();

    // Submit TLV as a bundle to the host bundle core
    bool ok = send_tlv(tlv);

    if (wait_info && ok)
        SET_STATE(WAIT_INFO);

    return ok;
}

bool
Encounter::send_tlv(ProphetTLV* tlv)
{
    // weed out the oddball
    if (tlv == NULL) return false;

    LOG(LOG_DEBUG,"send_tlv(%u) with %zu TLVs",tid_,tlv->size());

    // create a new facade bundle with src and dst
    // expire after HELLO_DEAD interval seconds
    // (convert from 100's of ms)
    Bundle* b = oracle_->core()->create_bundle(
                       tlv->source(),tlv->destination(),
                       oracle_->params()->hello_dead() *
                       oracle_->params()->hello_interval() / 10);

    // create a buffer to move data between Prophet and bundle host, with
    // some slush factor
    u_char* buf = new u_char[tlv->length() + 512];

    size_t len = tlv->serialize(buf,tlv->length() + 512);
    bool ok = oracle_->core()->write_bundle(b,buf,len);

    // clean up memory
    delete [] buf;
    delete tlv;

    if (ok)
    {
        ok = oracle_->core()->send_bundle(b, next_hop_);
        if (ok)
        {
            // update timestamp
            data_sent_ = time(0);
        }
        else
            LOG(LOG_ERR,"failed to send TLV");
    }

    return ok;
}

}; // namespace prophet

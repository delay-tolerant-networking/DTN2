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

#ifndef _PROPHET_ENCOUNTER_H_
#define _PROPHET_ENCOUNTER_H_

#include "Link.h"
#include "BaseTLV.h"
#include "HelloTLV.h"
#include "ProphetTLV.h"
#include "Oracle.h"
#include "Table.h"
#include "Dictionary.h"
#include "BundleTLVEntryList.h"
#include "Alarm.h"
#include <ctime>

namespace prophet
{

/**
 * Section 4.4.4, p. 28
 * The Routing Information Base lists the destinations a node knows of,
 * and the delivery predictabilities it has associated with them.  This
 * information is needed by the PRoPHET algorithm to make decisions on
 * routing and forwarding.
 *
 * Section 4.4.3, p. 27
 * The Routing Information Base Dictionary includes the list of addresses
 * used in making routing decisions.  The referents remain constant for
 * the duration of a session over a link where the instance numbers remain
 * the same and can be used by both the Routing Information Base messages
 * and the bundle offer messages.
 *
 * Track the state for the Prophet protocol throughout the duration of
 * this encounter between the local node and this remote.
 */
class Encounter : public ExpirationHandler
{
public:
    typedef enum {
        UNDEFINED_STATE = 0,
        WAIT_NB,    ///< Waiting for Neighbor
        SYNSENT,    ///< Sent SYN, waiting for SYNACK
        SYNRCVD,    ///< Received SYN, sent SYNACK, waiting for ACK
        ESTAB,      ///< Prophet link established with remote
        WAIT_DICT,  ///< Listener mode of bundle-passing phase
        WAIT_RIB,   ///< Listener rcvd RIBD, waiting for RIB
        OFFER,      ///< Listener sent Offer, waiting for Request
        CREATE_DR,  ///< Initiator creates and sends RIBD and RIB
        SEND_DR,    ///< Initiator sent RIBD and RIB, waiting for Offer
        REQUEST,    ///< Initiator sent Request, waiting for Bundles
        WAIT_INFO   ///< All phases now complete, waiting for timer or ACK
    } state_t;

    static const char* state_to_str(state_t st) {
        switch(st) {
#define CASE(_state) case _state: return # _state
        CASE(WAIT_NB);
        CASE(SYNSENT);
        CASE(SYNRCVD);
        CASE(ESTAB);
        CASE(WAIT_DICT);
        CASE(WAIT_RIB);
        CASE(OFFER);
        CASE(CREATE_DR);
        CASE(SEND_DR);
        CASE(REQUEST);
        CASE(WAIT_INFO);
#undef CASE
        default: return "Unknown State";
        }
    }
    /**
     * Constructor
     */
    Encounter(const Link* nexthop, Oracle* oracle, u_int16_t instance);

    /**
     * Copy constructor
     */
    Encounter(const Encounter& e);

    /**
     * Destructor
     */
    ~Encounter();

    ///@{ Operators
    bool operator< (const Encounter& e) const
    {
        return remote_instance_ < e.remote_instance_;
    }
    ///@}

    ///@{ Accessors
    u_int16_t   remote_instance() const { return remote_instance_; }
    u_int16_t   local_instance() const { return local_instance_; }
    const char* remote_eid() const { return next_hop_->remote_eid(); }
    const Link* nexthop() const { return next_hop_; }
    state_t     state() const { return state_; }
    const char* state_str() const { return state_to_str(state_); }
    bool        neighbor_gone() const { return neighbor_gone_; }
    u_int       time_remaining() const { return alarm_->time_remaining(); }
    ///@}

    /**
     * Callback to inform this instance that the hello_interval
     * parameter has changed
     */
    void hello_interval_changed();

    /**
     * Callback for this instance to receive TLVs received from peer
     * by the host bundling system. Encounter assumes ownership of
     * memory pointed to by tlv. Return true if message processed 
     * successfully.  Return false upon fault (peering session died).
     */
    bool receive_tlv(ProphetTLV* tlv);

    /**
     * Callback for timeout handler, either due to peer failure or
     * unacceptable delay in messaging
     */
    void handle_timeout();

    /**
     * Callback for tracking which Bundle requests are outstanding 
     */
    void handle_bundle_received(const Bundle* b);

protected:

    ///@{ TLV event handlers
    bool dispatch_tlv(BaseTLV* tlv);
    bool handle_hello_tlv(BaseTLV* hello);
    bool handle_ribd_tlv(BaseTLV* ribd);
    bool handle_rib_tlv(BaseTLV* rib);
    bool handle_offer_tlv(BaseTLV* offer);
    bool handle_response_tlv(BaseTLV* response);
    ///@}

    ///@{ Outbound message generators
    bool send_hello(HelloTLV::hello_hf_t hf,
                    ProphetTLV::header_result_t hr = ProphetTLV::NoSuccessAck,
                    u_int32_t tid = 0);
    bool send_dictionary_rib(ProphetTLV::header_result_t hr =
                                                    ProphetTLV::NoSuccessAck,
                             u_int32_t tid = 0);
    bool send_offer(ProphetTLV::header_result_t hr = ProphetTLV::NoSuccessAck,
                    u_int32_t tid = 0);
    bool send_response(ProphetTLV::header_result_t hr =
                                                     ProphetTLV::NoSuccessAck,
                       u_int32_t tid = 0);
    bool send_tlv(ProphetTLV* tlv);
    ///@}

    Oracle* const oracle_; ///< collection of Prophet information
    u_int16_t local_instance_; ///< local's instance for remote
    u_int16_t remote_instance_; ///< remote's instance for local 
    u_int32_t tid_; ///< transaction id from peer's most recent TLV
    u_int32_t next_tid_; ///< used to generate TID for outbound TLVs
    u_int32_t timeout_; ///< most milliseconds expected between TLVs
    const Link* next_hop_; ///< Link object for this encounter
    ProphetTLV* tlv_; ///< most recent message received from peer
    const bool synsender_; ///< whether active or passive during Hello phase
    state_t state_; ///< which phase of Prophet protocol for this end
    bool synsent_; ///< whether hello phase has sent SYN or SYNACK
    bool estab_; ///< whether hello phase has been completed
    volatile bool neighbor_gone_; ///< whether session has died
    Dictionary local_ribd_; ///< 16 bit index lookup to translate routes
    Dictionary remote_ribd_; ///< 16 bit index lookup for remote's routes
    BundleOfferList remote_offers_; ///< in-memory rep of remote's offer
    BundleResponseList local_response_; ///< Bundle requests sent to peer
    Table remote_nodes_; ///< in-memory representation of remote's RIB
    u_int hello_rate_; ///< simple flow control for Hello messages (ACK, etc)
    time_t data_sent_; ///< timestamp of last TLV sent
    time_t data_rcvd_; ///< timestamp of last TLV received
    Alarm* alarm_; ///< callback registration for timeout handler
}; // class Encounter

}; // namespace prophet

#endif // _PROPHET_ENCOUNTER_H_

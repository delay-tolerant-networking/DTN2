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

#ifndef _DTN_PROPHET_ENCOUNTER_
#define _DTN_PROPHET_ENCOUNTER_

#include <oasys/debug/Log.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/util/Time.h>
#include "naming/EndpointID.h"
#include "contacts/Link.h"
#include <sys/types.h> 

#include "routing/Prophet.h"
#include "routing/ProphetNode.h"
#include "routing/ProphetLists.h"
#include "routing/ProphetTLV.h"

#include "bundling/BundleList.h"
#include "bundling/BundleActions.h"

/**
 * Pages and paragraphs refer to IETF Prophet, March 2006
 */

namespace dtn {

class ProphetOracle {
public:
    virtual ~ProphetOracle() {}
    virtual ProphetParams*      params() = 0;
    virtual ProphetBundleQueue* bundles() = 0;
    virtual ProphetTable*       nodes() = 0;
    virtual BundleActions*      actions() = 0;
    virtual ProphetAckList*     acks() = 0;
    virtual ProphetStats*       stats() = 0;
};

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
class ProphetEncounter : public oasys::Thread,
                         public oasys::Logger
{
public:

    /**
     * Section 2.3 refers to NEW_NEIGHBOR and NEIGHBOR_GONE signals
     * The definition of ProphetEncounter is that each instance is
     * created in response to NEW_NEIGHBOR and shutdown in response
     * to NEIGHBOR_GONE.
     *
     * Section 5.2 refers to two independent events that affect the
     * state machine (implemented by ProphetEncounter): "the timer
     * expires, and a packet arrives".  This packet is a Prophet
     * control message, instantiated as ProphetTLV.
     */
    typedef enum {
        PEMSG_INVALID = 0,
        PEMSG_PROPHET_TLV_RECEIVED,
        PEMSG_HELLO_INTERVAL_CHANGED,
        PEMSG_NEIGHBOR_GONE,
    } pemsg_t;

    const char* pemsg_to_str(pemsg_t type) {
        switch(type) {
        case PEMSG_INVALID:              return "PEMSG_INVALID";
        case PEMSG_PROPHET_TLV_RECEIVED: return "PEMSG_PROPHET_TLV_RECEIVED";
        case PEMSG_HELLO_INTERVAL_CHANGED: return "PEMSG_HELLO_INTERVAL_CHANGED";
        case PEMSG_NEIGHBOR_GONE:        return "PEMSG_NEIGHBOR_GONE";
        default:                         PANIC("bogus pemsg_t");
        }
    }

    struct PEMsg {
        PEMsg()
            : type_(PEMSG_INVALID),
              tlv_(NULL) {}
        PEMsg(pemsg_t type,ProphetTLV* tlv = NULL)
            : type_(type), tlv_(tlv) {}

        pemsg_t type_;
        ProphetTLV* tlv_;
    };

    /**
     * @param nexthop Link on which new neighbor detected
     * @param controller parent caller
     */
    ProphetEncounter(const LinkRef& nexthop,
                     ProphetOracle* oracle);

private:
    /**
     * Deny access to copy constructor
     */
    ProphetEncounter(const ProphetEncounter& pe)
    :
        oasys::Thread("ProphetEncounter"),
        oasys::Logger("ProphetEncounter",pe.logpath_),
        cmdqueue_("/dtn/route/prophet/encounter"),
        to_be_fwd_("ProphetEncounter to be forwarded") {}

public:
    virtual ~ProphetEncounter();

    typedef enum {
        UNDEFINED = 0,
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
    } prophet_state_t;

    static const char* state_to_str(prophet_state_t st) {
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
        default:        PANIC("Unknown State");
        }
    }

    /**
     * Call-back function for ProphetController to deliver Prophet
     * control messages to this instance.
     */
    void receive_tlv(ProphetTLV*);

    /**
     * Link instance used by remote (sender_instance on inbound messages,
     * receiver_instance on outbound)
     */
    u_int16_t remote_instance() const { return remote_instance_; }

    /**
     * Link instance used by local (receiver_instance on inbound messages,
     * sender_instance on outbound)
     */
    u_int16_t local_instance()  const { return local_instance_; }

    const EndpointID& remote_eid() const { return next_hop_->remote_eid(); }

    bool operator< (const ProphetEncounter& p) const;
    bool operator< (u_int16_t instance) const;

    const LinkRef& next_hop() const {return next_hop_;}

    /**
     * Page 34, section 5.2<br>
     * The procedure "Reset the link" is defined as<br>
     * 1. Generate a new instance number for the link.<br>
     * 2. Delete the peer verifier (set to zero the values of Sender<br>
     *    Instance and Sender Local Address previously stored by the<br>
     *    Update Peer Verifier operation).<br>
     * 3. Send a SYN message.<br>
     * 4. Enter the SYNSENT state.<br>
     */

    void reset_link();

    /**
     * Page 13, section 2.3, Lower Layer Requirements and Interface
     */
    void neighbor_gone();

    /**
     * If a link changes to BUSY while sending, then bundles queue up.
     * When the link changes back to AVAILABLE, this signal indicates
     * to flush outbound queue.
     */
    void flush_pending() { fwd_to_nexthop(NULL); }

    void dump_state(oasys::StringBuffer* buf);
    void handle_bundle_received(Bundle*);
    void hello_interval_changed() {
        log_notice("hello_interval_changed");
        cmdqueue_.push_back(PEMsg(PEMSG_HELLO_INTERVAL_CHANGED));
    }
protected:

    /**
     * ProphetEncounter's main responsibility is to implement the 
     * Prophet protocol as described by Section 5.
     */
    void run();

    /**
     * Page 33, section 5.2
     * The "Update Peer Verifier" operation is defined as storing the
     * values of the Sender Instance and Sender Local Address fields from 
     * a Hello SYN or Hello SYNACK function received from the entity at 
     * the far end of the link.
     */
    void update_peer_verifier(u_int16_t instance) {
        remote_instance_ = instance;
        remote_addr_ = next_hop_->nexthop();
    }

    /**
     * Process command received from IPC via MsgQueue
     */
    void process_command();

    /**
     * Demultiplex TLV by typecode and dispatch to appropriate handler
     */
    void handle_prophet_tlv(ProphetTLV* pt);

    /**
     * Hello TLV handler
     */
    bool handle_hello_tlv(HelloTLV* hello,ProphetTLV* pt);

    /**
     * Dictionary TLV handler
     */
    bool handle_ribd_tlv(RIBDTLV* ribd,ProphetTLV* pt);

    /**
     * Delivery predictability TLV handler
     */
    bool handle_rib_tlv(RIBTLV* rib,ProphetTLV* pt);

    /**
     * Bundle offer/request TLV handler
     */
    bool handle_bundle_tlv(BundleTLV* btlv,ProphetTLV* pt);

    /**
     * Handles irregularities in protocol
     */
    bool handle_bad_protocol(u_int32_t tid);

    /**
     * Cleanup
     */
    void handle_neighbor_gone();

    /**
     * State-appropriate response to timeout
     */
    void handle_poll_timeout();

    /**
     * As per Prophet I-D, this implementation makes a single-threaded pass
     * through each of the Information Exchange phases (Initiator and
     * Listener); this method facilitates the switch between phases
     */
    void switch_info_role();

    /**
     * Reset local dictionary to 0 for sender, 1 for listener (as per
     * Hello phase roles)
     */
    void reset_ribd(const char* where);

    /**
     * Interrupt run() if necessary, to respond to console imperative
     */
    void handle_hello_interval_changed();

    /**
     * Package and send local dictionary and RIB
     */
    void send_dictionary();

    /**
     * Package and send Bundle offer (or request, as appropriate)
     */
    void send_bundle_offer();

    /**
     * Send an outbound TLV.  If tid is non-zero, set Prophet header 
     * to use it, otherwise generate a new tid.
     */
    ///@{
    void enqueue_hello(Prophet::hello_hf_t hf,
            u_int32_t tid = 0,
            Prophet::header_result_t result = Prophet::NoSuccessAck);
    void enqueue_ribd(const ProphetDictionary& ribd,
            u_int32_t tid = 0,
            Prophet::header_result_t result = Prophet::NoSuccessAck);
    void enqueue_rib(const RIBTLV::List& nodes,
            u_int32_t tid = 0,
            Prophet::header_result_t result = Prophet::NoSuccessAck);
    void enqueue_bundle_tlv(const BundleOfferList& list,
            u_int32_t tid = 0,
            Prophet::header_result_t result = Prophet::NoSuccessAck);
    ///@}
    
    /**
     * Create and return a new ProphetTLV
     */
    ProphetTLV* outbound_tlv(u_int32_t tid,
                             Prophet::header_result_t result);

    /**
     * Encapsulate outbound TLV into Bundle, hand off to Link for delivery
     */
    bool send_prophet_tlv();

    /**
     * Check link status to determine whether ok to forward
     */
    bool should_fwd(Bundle* bundle,bool hard_fail = true);

    /**
     * Given a bundle, forward to remote
     */
    void fwd_to_nexthop(Bundle* bundle,bool add_front = false);

    /**
     * Convenience function enforces valid state transitions and 
     * controls concurrent requests for access to state variable
     */
    ///@{
    void set_state(prophet_state_t);
    prophet_state_t get_state(const char* where);
    ///@}

    ProphetOracle* oracle_; ///< calling parent
    u_int16_t remote_instance_; ///< local's instance for remote
    std::string remote_addr_; ///< Sender Local Address for remote
    u_int16_t local_instance_; ///< remote's instance for local
    u_int32_t tid_; ///< transaction ID from peer's most recent TLV
    u_int32_t timeout_; ///< poll timeout
    u_int32_t ack_count_; ///< Section 5.2.1, Note 2, no more than 2 ACKs ...
    LinkRef next_hop_;  ///< Link object for this encounter
    bool synsender_; ///< whether active or passive during hello phase
    bool initiator_; ///< whether active or passive during information exchange
    bool synsent_;  ///< whether Hello SYN has been sent
    bool synrcvd_;  ///< whether Hello SYN has been received
    bool estab_;    ///< whether Hello sequence has been completed
    volatile bool neighbor_gone_; ///< indicates underlying CL signal
    prophet_state_t state_; ///< represents which phase of Prophet FSM
    ProphetDictionary ribd_; ///< dictionary for EID to StringID lookups
    BundleOfferList offers_; ///< List of offers received from remote
    BundleOfferList requests_; ///< List of offers requested from remote
    oasys::MsgQueue<PEMsg> cmdqueue_; ///< command dispatch queue
    ProphetTable remote_nodes_; ///< list of remote's p_values
    BundleList to_be_fwd_; ///< holding tank in case of blocked send
    Prophet::header_result_t result_; ///< whether a response is requested
    oasys::Time data_sent_; ///< last time a message was sent
    oasys::Time data_rcvd_; ///< last time a message was received
    ProphetTLV* outbound_tlv_; ///< outbound Prophet control messages
    oasys::SpinLock* state_lock_; ///< control access to state_ variable
    oasys::SpinLock* otlv_lock_; ///< control access to outbound_tlv_ variable
}; // ProphetEncounter

}; // dtn

#endif // _DTN_PROPHET_ENCOUNTER_

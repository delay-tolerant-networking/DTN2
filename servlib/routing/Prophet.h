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

#ifndef _DTN_PROPHET_
#define _DTN_PROPHET_

#include <sys/types.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/util/URL.h>
#include "naming/EndpointID.h"

/**
 * Pages and paragraphs refer to IETF Prophet, March 2006
 * The designation, (arbitrary), means the implementor's discretion ;-)
 */

namespace dtn {

struct Prophet {

    /**
     * Default initialization values, p. 15, 3.3, figure 2
     */
    static const double DEFAULT_P_ENCOUNTER   = 0.75;

    /**
     * Default initialization values, p. 15, 3.3, figure 2
     */
    static const double DEFAULT_BETA  = 0.25;

    /**
     * Default initialization values, p. 15, 3.3, figure 2
     */
    static const double DEFAULT_GAMMA = 0.99;

    /**
     * The kappa variable describes how many 
     * milliseconds-per-timeunit (for equation 2, p.9, section 2.1.1)
     */
    static const u_int DEFAULT_KAPPA  =  100;

    // initial timer values (in seconds)

    /**
     * Time between HELLO beacons (in 100ms units)
     */
    static const u_int8_t HELLO_INTERVAL = 255; ///< 25.5 sec (arbitrary)

    /**
     * Max units of HELLO_INTERVAL before peer is considered unreachable
     */
    static const u_int HELLO_DEAD     = 20; ///< 8.5 min (arbitrary)

    /**
     * Max times to forward a bundle for GTMX
     */
    static const u_int DEFAULT_NUM_F_MAX = 5; ///< arbitrary

    /**
     * Min times to forward a bundle for LEPR
     */
    static const u_int DEFAULT_NUM_F_MIN = 3; ///< arbitrary

    /**
     * Seconds between aging of nodes and Prophet ACKs
     */
    static const u_int AGE_PERIOD = 1800; ///< arbitrary

    /**
     * Current version of the protocol
     */
    static const u_int8_t PROPHET_VERSION = 0x01;

    /**
     * Header Definition
     * p. 21, 4.2
     */
    struct ProphetHeader {
        u_int8_t version; ///< This version of the PRoPHET Protocol = 1.
        u_int8_t flags; ///< TBD
        /**
         * Field  that is used to indicate whether a response is required
         * to the request message if the outcome is successful.  A value of
         * "NoSuccessAck" indicates that the request message does not
         * expect a response if the outcome is successful, and a value of
         * "AckAll" indicates that a response is expected if the outcome is
         * successful.  In both cases a failure response MUST be generated
         * if the request fails.<br><br>
         * In a response message, the result field can have two values:
         * "Success," and "Failure".  The "Success" results indicates a
         * success response.  All messages that belong to the same success
         * response will have the same Transaction Identifier.  The
         * "Success" result indicates a success response that may be
         * contained in a single message or the final message of a success
         * response spanning multiple messages.<br><br>
         * ReturnReceipt is a result field used to indicate that an
         * acknowledgement is required for the message.  The default for
         * Messages is that the controller will not acknowledge responses.
         * In the case where an acknowledgement is required, it will set
         * the Result Field to ReturnReceipt in the header of the Message.
         * <br><br>
         * The encoding of the result field is:<br>
         * <br>
         *            NoSuccessAck:       Result = 1<br>
         *            AckAll:             Result = 2<br>
         *            Success:            Result = 3<br>
         *            Failure:            Result = 4<br>
         *            ReturnReceipt       Result = 5<br>
         * <br>
         */
        u_int8_t result;
        /**
         * Field gives further information concerning the result in a
         * response message.  It is mostly used to pass an error code in a
         * failure response but can also be used to give further
         * information in a success response message or an event message.
         * In a request message, the code field is not used and is set to
         * zero.<br><br>
         * If the Code field indicates that the Error TLV is included in
         * the message, further information on the error will be found in
         * the Error TLV, which MUST be the the first TLV after the header.
         * <br><br>
         * The encoding is:<br>
         * <br>
         *     PRoPHET Error messages       0x000 - 0x099<br>
         *     Reserved                     0x0A0 - 0x0FE<br>
         *     Error TLV in message             0x0FF<br>
         * 
         */
        u_int8_t code;
        /**
         * For messages during the Hello phase with the Hello SYN, Hello
         * SYNACK, and Hello ACK functions, it is the sender's instance
         * number for the link.  It is used to detect when the link comes
         * back up after going down or when the identity of the entity at
         * the other end of the link changes.  The instance number is a 16-
         * bit number that is guaranteed to be unique within the recent
         * past and to change when the link or node comes back up after
         * going down.  Zero is not a valid instance number.  For the
         * RSTACK function, the Sender Instance field is set to the value
         * of the Receiver Instance field from the incoming message that
         * caused the RSTACK function to be generated.  Messages sent after
         * the Hello phase is completed should use the sender's instance
         * number for the link.
         */
        u_int16_t sender_instance;
        /**
         * For messages during the Hello phase with the Hello SYN, Hello
         * SYNACK, and Hello ACK functions, is what the sender believes is
         * the current instance number for the link, allocated by the
         * entity at the far end of the link.  If the sender of the message
         * does not know the current instance number at the far end of the
         * link, this field SHOULD be set to zero.  For the RSTACK message,
         * the Receiver Instance field is set to the value of the Sender
         * Instance field from the incoming message that caused the RSTACK
         * message to be generated.  Messages sent after the Hello phase is
         * completed should use what the sender believes is the current
         * instance number for the link, allocated by the entity at the far
         * end of the link.
         */
        u_int16_t receiver_instance;
        /**
         * Used to associate a message with its response message.  This
         * should be set in request messages to a value that is unique for
         * the sending host within the recent past.  Reply messages contain
         * the Transaction Indentifier of the request they are responding to.
         */
        u_int32_t transaction_id;
        /**
         * If S is set then the SubMessage Number field indicates the total
         * number of SubMessage segments that compose the entire message.
         * If it is not set then the SubMessage  Number field indicates the
         * sequence number of this SubMessage segment within the whole
         * message. the S field will only be set in the first sub-message
         * of a sequence.
         */
        u_int16_t submessage_flag:1;
        /**
         * When a message is segmented because it exceeds the MTU of the
         * link layer, each segment will include a submessage number to
         * indicate its position.  Alternatively, if it is the first
         * submessage in a sequence of submessages, the S flag will be set
         * and this field will contain the total count of submessage
         * segments.
         */
        u_int16_t submessage_num:15;
        /**
         * Length in octets of this message including headers and message
         * body.  If the message is fragmented, this field contains the
         * length of this submessage.
         */
        u_int16_t length;
    } __attribute__((packed));

    /**
     * Legal values for ProphetHeader.result field
     * p. 22, 4.2
     */
    typedef enum {
        UnknownResult = 0x0,
        NoSuccessAck  = 0x1,
        AckAll        = 0x2,
        Success       = 0x3,
        Failure       = 0x4,
        ReturnReceipt = 0x5
    } header_result_t;

    typedef enum {
        UNKNOWN_TLV = 0x00,
        HELLO_TLV   = 0x01,
        ERROR_TLV   = 0x02,
        RIBD_TLV    = 0xA0,
        RIB_TLV     = 0xA1,
        BUNDLE_TLV  = 0XA2, 
    } prophet_tlv_t;

    /**
     * Hello TLV header<br>
     * p. 25, 4.4.1<br>
     * <br>
     * The Hello TLV is used to set up and maintain a link between two
     * PRoPHET nodes.  Hello messages with the SYN function are transmitted
     * periodically as beacons.  The Hello TLV is the first TLV exchanged
     * between two PRoPHET nodes when they encounter each other.  No other
     * TLVs can be exchanged until the first Hello sequenece is completed.<br>
     * <br>
     * Once a communication link is established between two PRoPHET nodes,
     * the Hello TLV will be sent once for each interval as defined in the
     * interval timer.  If a node experiences the lapse of HELLO_DEAD Hello
     * intervals without receiving a Hello TLV on an ESTAB connection (as
     * defined in the state machine in Section 5.2), the connection SHOULD
     * be assumed broken.
     */
    struct HelloTLVHeader {
        u_int8_t type; ///< defined as 0x01
        u_int8_t unused__:5;
        /**
         * Specifies the function of the Hello TLV.  Four functions are
         * specified for the Hello TLV:
         * <br>
         *       SYN:     HF = 1<br>
         *       SYNACK:  HF = 2<br>
         *       ACK:     HF = 3<br>
         *       RSTACK:  HF = 4.<br>
         */
        u_int8_t HF:3;
        /**
         * Length of the TLV in octets, including the TLV header and any
         * nested TLVs.
         */
        u_int16_t length;
        /**
         * The Timer field is used to inform the receiver of the timer
         * value used in the Hello processing of the sender.  The timer
         * specifies the nominal time between periodic Hello messages.  It
         * is a constant for the duration of a session.  The timer field is
         * specified in units of 100ms.
         */
        u_int8_t timer;
        /**
         * The Name Length field is used to specify the length of the
         * Sender Name field in octets.  If the name has already been sent
         * at least once in a message with the current Sender Instance, a
         * node MAY choose to set this field to zero, omitting the Sender
         * Name from the Hello TLV.
         */
        u_int8_t name_length;
        /**
         * The Sender Name field specifies the routable DTN name of the
         * sender that is to be used in updating routing information and
         * making forwarding decisions.
         */
        u_char sender_name[0];
    } __attribute__((packed));

    /**
     * Legal values for HelloTLVHeader.HF (hello function)
     * p. 25, 4.4.1
     */
    typedef enum {
        HF_UNKNOWN  = 0x0,
        SYN         = 0x1,
        SYNACK      = 0x2,
        ACK         = 0x3,
        RSTACK      = 0x4
    } hello_hf_t;

    /**
     * Error TLV header
     * p. 26, 4.4.2
     */
    struct ErrorTLVHeader {
        u_int8_t type; ///< defined as 0x02
        u_int8_t flags; ///< TBD
        /**
         * Length of the TLV in octets, including the TLV header and any
         * nested TLVs.
         */
        u_int16_t length;
    } __attribute__((packed));

    /**
     * Routing Information Base Dictionary TLV<br>
     * p. 27, 4.4.3<br>
     * <br>
     * The Routing Information Base Dictionary includes the list of
     * addresses used in making routing decisions.  The referents remain
     * constant for the duration of a session over a link where the instance
     * numbers remain the same and can be used by both the Routing
     * Information Base messages and the bundle offer messages.
     */
    struct RIBDTLVHeader {
        u_int8_t type; ///< defined as 0xA0
        u_int8_t flags; ///< TBD
        /**
         * Length of the TLV in octets, including the TLV header and any
         * nested TLVs.
         */
        u_int16_t length;
        u_int16_t entry_count; ///< Number of entries in the database
        u_int16_t unused__;
    } __attribute__((packed));

    /**
     * Routing Address String (entry in RIBD above)
     * p. 27, 4.4.3
     */
    struct RoutingAddressString {
        /**
         * 16 bit identifier that is constant for the duration of a
         * session.  String ID zero is predefined as the node initiating
         * the session through sending the Hello SYN message, and String ID
         * one is predefined as the node responding with the Hello SYNACK
         * message.
         */
        u_int16_t string_id;
        u_int8_t length; ///< Length of Address String.
        u_int8_t unused__;
        u_char ra_string[0];
    } __attribute__((packed));

    /**
     * Routing Information Base TLV <br>
     * p. 28, 4.4.4 <br>
     * <br>
     * The Routing Information Base lists the destinations a node knows of,
     * and the delivery predictabilities it has associated with them.  This
     * information is needed by the PRoPHET algorithm to make decisions on
     * routing and forwarding.
     */
    struct RIBTLVHeader {
        u_int8_t type; ///< defined as 0xA1
        /**
         * The encoding of the Header flag field relates to the
         * capabilities of the Source node sending the RIB:
         * <br>
         *       Flag 0: Relay Node          0b1 <br>
         *       Flag 1: Custody Node        0b1 <br>
         *       Flag 2: Internet GW Node    0b1 <br>
         *       Flag 3: Reserved            0b1 <br>
         *       Flag 4: Reserved            0b1 <br>
         *       Flag 5: Reserved            0b1 <br>
         *       Flag 6: Reserved            0b1 <br>
         *       Flag 7: Reserved            0b1 <br>
         */
        u_int8_t flags;
        /**
         * Length of the TLV in octets, including the TLV header and any
         * nested TLVs.
         */
        u_int16_t length;
        u_int16_t rib_string_count; ///< Number of routing entries in the TLV
        u_int16_t unused__;
    } __attribute__((packed));

    /** 
     * RIB Header Flags
     * p. 28, 4.4.4
     */
    typedef enum {
        RELAY_NODE       = 1 << 0,
        CUSTODY_NODE     = 1 << 1,
        INTERNET_GW_NODE = 1 << 2
    } rib_header_flag_t;

    /** 
     * Routing Information Base entry
     * p. 28, 4.4.4
     */
    struct RIBEntry {
        /**
         * ID string as predefined in the dictionary TLV.
         */
        u_int16_t string_id;
        /**
         * Delivery predictability for the destination of this entry as
         * calculated according to the equations in Section 2.1.1.  The
         * encoding of this field is a linear mapping from [0,1] to [0,
         * 0xFF].
         */
        u_int8_t pvalue;
        /**
         * The encoding of the RIB flag field is<br>
         * <br>
         *       Flag 0: Relay Node          0b1 <br>
         *       Flag 1: Custody Node        0b1 <br>
         *       Flag 2: Internet GW Node    0b1 <br>
         *       Flag 3: Reserved            0b1 <br>
         *       Flag 4: Reserved            0b1 <br>
         *       Flag 5: Reserved            0b1 <br>
         *       Flag 6: Reserved            0b1 <br>
         *       Flag 7: Reserved            0b1 <br>
         */
        u_int8_t flags;
    } __attribute__((packed));

    /**
     * Bundle Offer/Response Header<br>
     * p. 30, 4.4.5<br>
     * <br>
     * After the routing information has been passed, the node will ask the
     * other node to review available bundles and determine which bundles it
     * will accept for relay.  The source relay will determine which bundles
     * to offer based on relative delivery predictabilities as explained in
     * Section 3.6.  The Bundle Offer TLV also lists the bundles that a
     * PRoPHET acknowledgement has been issued for.  Those bundles have the
     * PRoPHET ACK flag set in their entry in the list.  When a node
     * receives a PRoPHET ACK for a bundle, it MUST remove any copies of
     * that bundle from its buffers, but SHOULD keep an entry of the
     * acknowledged bundle to be able to further propagate the PRoPHET ACK.
     * <br><br>
     * The Response message is identical to the request message with the
     * exception that the flag indicate acceptance of the bundle.
     */
    struct BundleOfferTLVHeader {
        u_int8_t type; ///< defined as 0xA2
        u_int8_t flags; ///< TBD
        /**
         * Length of the TLV in octets, including the TLV header and any
         * nested TLVs.
         */
        u_int16_t length;
        /**
         * Number of bundle offer entries.
         */
        u_int16_t offer_count;
        u_int16_t unused__;
    } __attribute__((packed));

    /**
     * Bundle Offer/Response Entry
     * p. 30, 4.4.5
     */
    struct BundleOfferEntry {
        /**
         * ID string of the destination of the bundle as predefined in the
         * dictionary TLV.
         */
        u_int16_t dest_string_id;
        /**
         * The encoding of the B_Flags in the request are: <br>
         * <br>
         *      Flag 0: Custody Offered     0b1 <br>
         *      Flag 1: Reserved            0b1 <br>
         *      Flag 2: Reserved            0b1 <br>
         *      Flag 3: Reserved            0b1 <br>
         *      Flag 4: Reserved            0b1 <br>
         *      Flag 5: Reserved            0b1 <br>
         *      Flag 6: Reserved            0b1 <br>
         *      Flag 7: PRoPHET ACK         0b1 <br>
         * <br>
         * The encoding of the B_flag values in the response are: <br>
         * <br>
         *      Flag 0: Custody Accepted    0b1 <br>
         *      Flag 1: Bundle Accepted     0b1 <br>
         *      Flag 2: Reserved            0b1 <br>
         *      Flag 3: Reserved            0b1 <br>
         *      Flag 4: Reserved            0b1 <br>
         *      Flag 5: Reserved            0b1 <br>
         *      Flag 6: Reserved            0b1 <br>
         *      Flag 7: Reserved            0b1 <br>
         * <br> 
         */
        u_int8_t b_flags;
        u_int8_t unused__;
        u_int32_t creation_timestamp; ///< This bundle's creation timestamp
        u_int32_t seqno; ///< NOT IN SPEC
    } __attribute__((packed));

    typedef struct BundleOfferTLVHeader BundleResponseTLVHeader;
    typedef struct BundleOfferEntry BundleResponseEntry;

    /**
     * BundleOffer flag values
     * p. 31, 4.4.5
     */
    typedef enum {
        CUSTODY_OFFERED = 1 << 0,
        PROPHET_ACK     = 1 << 7
    } bundle_offer_flags_t;

    /**
     * BundleResponse flag values
     * p. 31, 4.4.5
     */
    typedef enum {
        CUSTODY_ACCEPTED = 1 << 0,
        BUNDLE_ACCEPTED  = 1 << 1
    } bundle_response_flags_t;

    /**
     * Forwarding strategies
     * p. 17, 3.6
     */
    typedef enum {
        INVALID_FS = 0,
        GRTR,
        GTMX,
        GRTR_PLUS,
        GTMX_PLUS,
        GRTR_SORT,
        GRTR_MAX
    } fwd_strategy_t;

    /**
      * Queuing policies
      * p. 18, 3.7
      */
    typedef enum {
        INVALID_QP = 0,
        FIFO,
        MOFO,
        MOPR,
        LINEAR_MOPR,
        SHLI,
        LEPR
    } q_policy_t;

    /**
     * Utility class to generate the transaction ID (tid) and instance
     * numbers required by ProphetHeader
     */
    class UniqueID {
    public:
        static u_int32_t tid() {
            return instance()->get_tid();
        }

        static u_int16_t instance_id() {
            return instance()->get_instance_id();
        }

        static UniqueID* instance() {
            if (instance_ == NULL)
                PANIC("UniqueID::init not called yet");
            return instance_;
        }

        static void init() {
            if (instance_ != NULL)
                PANIC("UniqueID already initialized");
            instance_ = new UniqueID();
        }
    protected:
        UniqueID() :
            tid_(1),
            iid_(0),
            lock_(new oasys::SpinLock()) {}
        ~UniqueID() { delete lock_; }

        u_int32_t get_tid() {
            oasys::ScopeLock l(lock_,"Prophet::UniqueID::tid()");
            return tid_++;
        }
        u_int16_t get_instance_id() {
            oasys::ScopeLock l(lock_,"Prophet::UniqueID::instance()");
            iid_++;
            // Zero is not a valid instance number.
            if (iid_  == 0)
                iid_++;
            return iid_;
        }
        u_int32_t tid_;
        u_int16_t iid_;
        oasys::SpinLock* lock_;
        static UniqueID* instance_;
    }; // UniqueID

    static const size_t ProphetHeaderSize =
        sizeof(struct ProphetHeader);

    static const size_t HelloTLVHeaderSize =
        sizeof(struct HelloTLVHeader);

    static const size_t ErrorTLVHeaderSize =
        sizeof(struct ErrorTLVHeader);

    static const size_t RIBDTLVHeaderSize =
        sizeof(struct RIBDTLVHeader);

    static const size_t RoutingAddressStringSize =
        sizeof(struct RoutingAddressString);

    static const size_t RIBTLVHeaderSize =
        sizeof(struct RIBTLVHeader);

    static const size_t RIBEntrySize =
        sizeof(struct RIBEntry);

    static const size_t BundleOfferTLVHeaderSize =
        sizeof(struct BundleOfferTLVHeader);

    static const size_t BundleResponseTLVHeaderSize =
        BundleOfferTLVHeaderSize;

    static const size_t BundleOfferEntrySize =
        sizeof(struct BundleOfferEntry);

    static const size_t BundleReponseEntrySize =
        BundleOfferEntrySize;

    static const char*
    tlv_to_str(Prophet::prophet_tlv_t tlv)
    {
        switch(tlv) {
            case Prophet::HELLO_TLV:  return "HELLO_TLV";
            case Prophet::RIBD_TLV:   return "RIBD_TLV";
            case Prophet::RIB_TLV:    return "RIB_TLV";
            case Prophet::BUNDLE_TLV: return "BUNDLE_TLV";
            case Prophet::ERROR_TLV:
            case Prophet::UNKNOWN_TLV:
            default:
                PANIC("Unimplemented prophet typecode %u",tlv);
        }
    }

    static const char*
    result_to_str(Prophet::header_result_t hr)
    {
        switch(hr) {
            case Prophet::NoSuccessAck:  return "NoSuccessAck";
            case Prophet::AckAll:        return "AckAll";
            case Prophet::Success:       return "Success";
            case Prophet::Failure:       return "Failure";
            case Prophet::ReturnReceipt: return "ReturnReceipt";
            case Prophet::UnknownResult:
            default:
                PANIC("Undefined prophet header result: %d",hr);
        }
    }

    static const char*
    hf_to_str(Prophet::hello_hf_t hf)
    {
        switch(hf) {
            case Prophet::SYN:    return "SYN";
            case Prophet::SYNACK: return "SYNACK";
            case Prophet::ACK:    return "ACK";
            case Prophet::RSTACK: return "RSTACK";
            case Prophet::HF_UNKNOWN:
            default:
                PANIC("Unrecognized prophet HF code %u",hf);
        }
    }

    static const char*
    fs_to_str(Prophet::fwd_strategy_t fs)
    {
        switch(fs) {
#define CASE(_f_s) case Prophet::_f_s: return # _f_s
        CASE(GRTR);
        CASE(GTMX);
        CASE(GRTR_PLUS);
        CASE(GTMX_PLUS);
        CASE(GRTR_SORT);
        CASE(GRTR_MAX);
#undef CASE
        default: return "Unknown forwarding strategy";
        }
    }

    static const char*
    qp_to_str(Prophet::q_policy_t qp)
    {
        switch(qp) {
#define CASE(_q_p) case Prophet::_q_p: return # _q_p
        CASE(FIFO);
        CASE(MOFO);
        CASE(MOPR);
        CASE(LINEAR_MOPR);
        CASE(SHLI);
        CASE(LEPR);
#undef CASE
        default: return "Unknown queuing policy";
        }
    }

    /**
     * Utility function to convert Bundle destination to router id
     */
    static EndpointID eid_to_routeid(const EndpointID& eid)
    { 
        oasys::URL eid_url(eid.str());
        EndpointID routeid;
        routeid.assign(eid.scheme_str(),"//" + eid_url.host_);
        return routeid;
    }

    /**
     * Utility function to convert Bundle destination to implicit route
     */
    static EndpointIDPattern eid_to_route(const EndpointID& eid)
    { 
        EndpointID route = eid_to_routeid(eid);
        route.assign(eid.str() + "/*");
        return route;
    }

    /**
     * Convenience function to compute whether EID is a route to local
     */
     static bool route_to_me(const EndpointID& eid);

}; // struct Prophet

}; // dtn

#endif // _DTN_PROPHET_

#ifndef _DTN_PROPHET_ALGORITHM_
#define _DTN_PROPHET_ALGORITHM_

// pages and paragraphs refer to IETF Prophet, March 2006

namespace dtn {

class Prophet {
public:

    // default initialization values, p. 15, 3.3, figure 2
    static const double DEFAULT_P_ENCOUNTER   = 0.75;
    static const double DEFAULT_PROPHET_BETA  = 0.25;
    static const double DEFAULT_PROPHET_GAMMA = 0.25;

    /**
      * Current version of the protocol
      */
    static const u_int8_t PROPHET_VERSION = 0x01;

    // Strategy defaults are arbitrary; others are as defined by Prophet
    Prophet()
        : encounter_(DEFAULT_P_ENCOUNTER),
          beta_(DEFAULT_PROPHET_BETA),
          gamma_(DEFAULT_PROPHET_GAMMA),
          f_strategy(GRTR),
          q_strategy(FIFO)
    {
    }

    // p. 9, 2.1.1, equation 1
    /**
     * previous represents P_(A,B)_old
     * return value is P_(A,B)
     */
    double delivery_predictability(double previous) {
        ASSERT(previous >= 0.0 && previous <= 1.0);
        return (previous + (1.0 - previous) * encounter_);
    }

    // p. 9, 2.1.1, equation 2
    /**
     * previous represents P_(A,B)_old
     * return value is P_(A,B)
     */
    double age(double previous, u_int time_units) {
        ASSERT(previous >= 0.0 && previous <= 1.0);
        ASSERT(time_units > 0);
        return (previous * pow( gamma_, time_units+0.0));
    }

    // p. 10, 2.1.1, equation 3
    /**
     * previous represents P_(A,B)_old
     * return value is P_(A,B)
     */
    double transitivity(double previous, double ab, double bc) {
        ASSERT(previous >= 0.0 && previous <= 1.0);
        ASSERT(ab >= 0.0 && ab <= 1.0);
        ASSERT(bc >= 0.0 && bc <= 1.0);
        return (previous + (1 - previous) * ab * bc * beta_);
    }

    // p. 18, 3.7, equation 7
    /**
     * previous represents FAV_old
     * prob represents P_(A,B)
     * return value is FAV_new
     */
    double favorability(double previous, double prob) {
        ASSERT(previous >= 0.0 && previous <= 1.0);
        ASSERT(prob >= 0.0 && prob <= 1.0);
        return (previous + (1 - previous) * prob);
    }

    // forwarding strategies
    // p. 17, 3.6
    typedef enum {
        GRTR,
        GTMX,
        GRTR_PLUS,
        GTMX_PLUS,
        GRTR_SORT,
        GRTR_MAX
    } fwd_strategy_t;

    // queuing strategies
    // p. 18, 3.7
    typedef enum {
        FIFO,
        MOFO,
        MOPR,
        LINEAR_MOPR,
        SHLI,
        LEPR
    } q_strategy_t;

    // XXX/wilson Endian-ness beware!!
    //   all of the below should be audited and rewritten
    //   to be cross-platform-friendly

    // Header Definition
    // p. 21, 4.2
    struct ProphetHeader {
        u_int8_t version; // defined as 0x01
        u_int8_t flags;
        u_int8_t result;
        u_int8_t code;
        u_int16_t receiver_instance;
        u_int16_t sender_instance;
        u_int32_t transaction_id;
        u_int16_t submessage_flag:1;
        u_int16_t submessage_num:15;
        u_int16_t length;
    } __attribute__((packed));

    // Legal values for ProphetHeader.result field
    // p. 22, 4.2
    typedef enum {
        NoSuccessAck  = 0x1,
        AckAll        = 0x2,
        Success       = 0x3,
        Failure       = 0x4,
        ReturnReceipt = 0x5
    } header_result_t;

    // Hello TLV header
    // p. 25, 4.4.1
    struct HelloTLVHeader {
        u_int8_t type; // defined as 0x01
        u_int8_t unused__:5;
        u_int8_t HF:3;
        u_int16_t length;
        u_int8_t timer; // units of 100ms
        u_int8_t name_length;
        u_char sender_name[0];
    } __attribute__((packed));

    // Legal values for HelloTLVHeader.HF (hello function)
    // p. 25, 4.4.1
    typedef enum {
        SYN         = 0x1,
        SYNACK      = 0x2,
        ACK         = 0x3,
        RSTACK      = 0x4
    } hello_hf_t;

    // Error TLV header
    // p. 26, 4.4.2
    struct ErrorTLVHeader {
        u_int8_t type; // defined as 0x02
        u_int8_t flags; // TBD
        u_int16_t length;
    } __attribute__((packed));

    // Routing Information Base Dictionary TLV
    // p. 27, 4.4.3
    struct RIBDTLVHeader {
        u_int8_t type; // defined as 0xA0
        u_int8_t flags;
        u_int16_t length;
        u_int16_t ribd_entry_count;
        u_int16_t unused__;
    } __attribute__((packed));

    // Routing Address String (entry in RIBD above)
    // p. 27, 4.4.3
    struct RoutingAddressString {
        u_int16_t string_id;
        u_int8_t length;
        u_int8_t unused__;
        u_char ra_string[0];
    } __attribute__((packed));

    // Routing Information Base TLV
    // p. 28, 4.4.4
    struct RIBTLVHeader {
        u_int8_t type; // defined as 0xA1
        u_int8_t flags;
        u_int16_t length;
        u_int16_t rib_string_count;
        u_int16_t unused__;
    } __attribute__((packed));

    // RIB Header Flags
    // p. 28, 4.4.4
    typedef enum {
        RELAY_NODE       = 1 << 0,
        CUSTODY_NODE     = 1 << 1,
        INTERNET_GW_NODE = 1 << 2
    } rib_header_flag_t;

    // Routing Information Base entry
    // p. 28, 4.4.4
    struct RIBEntry {
        u_int16_t ribd_string_id;
        u_int8_t pvalue;
        u_int8_t flags;
    } __attribute__((packed));

    // RIB Entry Flags
    // p. 28, 4.4.4
    typedef enum {
        RELAY_NODE       = 1 << 0,
        CUSTODY_NODE     = 1 << 1,
        INTERNET_GW_NODE = 1 << 2
    } rib_entry_flag_t;

    // Bundle Offer/Response Header
    // p. 30, 4.4.5
    struct BundleOfferResponseHeader {
        u_int8_t type; // defined as 0xA2
        u_int8_t flags;
        u_int16_t length;
        u_int16_t bundle_offer_count;
        u_int16_t unused__;
    } __attribute__((packed));

    // Bundle Offer/Response Entry
    // p. 30, 4.4.5
    struct BundleOfferResponseEntry {
        u_int16_t bundle_dest_string_id;
        u_int8_t b_flags;
        u_int8_t unused__;
        u_int32_t creation_timestamp;
    } __attribute__((packed));

    // BundleOffer flag values
    // p. 31, 4.4.5
    typedef enum {
        CUSTODY_OFFERED = 1 << 0,
        PROPHET_ACK     = 1 << 7
    } bundle_offer_flags_t;

    // BundleResponse flag values
    // p. 31, 4.4.5
    typedef enum {
        CUSTODY_ACCEPTED = 1 << 0,
        BUNDLE_ACCEPTED  = 1 << 1
    } bundle_response_flags_t;

    // accessors
    double encounter() { return encounter_; }
    double beta() { return beta_; }
    double gamma() { return gamma_; }
    fwd_strategy_t fwd_strategy() { return f_strategy_; }
    q_strategy_t q_strategy() { return q_strategy_; }

    // mutators
    bool encounter( double d ) {
        if ( d >= 0.0 && d <= 1.0 ) {
            encounter_ = d;
            return true;
        }
        return false;
    }
    bool beta( double d ) {
        if ( d >= 0.0 && d <= 1.0 ) {
            beta_ = d;
            return true;
        }
        return false;
    }
    bool gamma( double d ) {
        if ( d >= 0.0 && d <= 1.0 ) {
            gamma_ = d;
            return true;
        }
        return false;
    }
    void fwd_strategy( fwd_strategy_t f ) { f_strategy_ = f; }
    void q_strategy( q_strategy_t q ) { q_strategy_ = q; }
protected:
    double encounter_;
    double beta_;
    double gamma_;
    fwd_strategy_t f_strategy_;
    q_strategy_t q_strategy_;
};

/**
 * ProphetNode stores state for a remote node as identified by remote_eid
 */
class ProphetNode
{
public:
    /**
     * Tunable parameters structure for tracking all the PRoPHET variables
     */
    class Params {
    public:
        double encounter_;
        double beta_;
        double gamma_;
    };

    Prophet(EndpointID remote);
    ~Prophet();
}; // Prophet

/**
 *  From p. 12, 2.2:
 *
 *  [T}he bundle agent needs to provide the following interface/functionality
 *  to the routing agent:
 *
 *  Get Bundle List
 *      Returns a list of the stored bundles and their attributes to the
 *      routing agent.
 *  Send Bundle
 *      Makes the bundle agent send a specified bundle.
 *  Accept Bundle
 *      Gives the bundle agent a new bundle to store.
 *  Bundle Delivered
 *      Tells the bundle agent that a bundle was delivered to its
 *      destination.
 *  Drop Bundle
 *      Makes the bundle agent drop a specified bundle.
 */

class ProphetTable 
{
public:
    ProphetTable();
    ~ProphetTable();
}; // ProphetTable

}; // dtn

#endif // _DTN_PROPHET_ALGORITHM_

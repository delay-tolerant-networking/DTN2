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

#ifndef _DTN_PROPHET_NODE_
#define _DTN_PROPHET_NODE_

#include "routing/Prophet.h"
#include <oasys/debug/Log.h>
#include <oasys/util/Time.h>
#include <oasys/util/URL.h>
#include "naming/EndpointID.h"
#include "bundling/BundleEvent.h"

#include <vector>
#include <map>
#include <set>

/**
 * Pages and paragraphs refer to IETF Prophet, March 2006
 */

namespace dtn {

struct ProphetNodeParams
{
    double encounter_; ///< initialization for p_value
    double beta_;      ///< transivity weight
    double gamma_;     ///< aging weight
    u_int   kappa_;    ///< milliseconds per unit time
};

/**
 * ProphetNode stores state for a remote node as identified by remote_eid
 */
class ProphetNode : public oasys::Logger
{
public:

    ProphetNode(ProphetNodeParams* params = NULL,
            const char* logpath = "/dtn/route/prophet/node");
    ProphetNode(const ProphetNode& node);

    virtual ~ProphetNode() {}

    double p_value() const {return p_value_;}
    bool relay() const {return relay_;}
    bool custody() const {return custody_;}
    bool internet_gw() const {return internet_gateway_;}
    EndpointID remote_eid() const {return remote_eid_;}
    const oasys::Time& age() const {return age_;}
    bool route_to_me(const EndpointID& eid) const;

    void set_eid( const EndpointID& eid ) { remote_eid_.assign(eid); }
    void set_pvalue( double d ) { 
        if ( d >= 0.0 && d <= 1.0 ) p_value_ = d;
    }
    void set_relay( bool relay ) { relay_ = relay; }
    void set_custody( bool custody ) { custody_ = custody; }
    void set_internet_gw( bool gw ) { internet_gateway_ = gw; }
    void set_age( oasys::Time t ) {age_.sec_=t.sec_; age_.usec_=t.usec_;}
    void set_age_now() { age_.get_time(); }

    ProphetNode& operator= (const ProphetNode& p) {
        p_value_ = p.p_value_;
        relay_   = p.relay_;
        custody_ = p.custody_;
        internet_gateway_ = p.internet_gateway_;
        remote_eid_.assign(p.remote_eid_);
        age_.sec_ = p.age_.sec_;
        age_.usec_ = p.age_.usec_;
        return *this;
    }

    double encounter() const {return params_->encounter_;}
    double beta() const {return params_->beta_;}
    double gamma() const {return params_->gamma_;}
    u_int  age_factor() const {return params_->kappa_;}
    /**
     * We have just seen this node, update delivery predictability metric
     * p. 9, 2.1.1, equation 1
     */
    void update_pvalue();

    /**
     * We have just seen a RIB containing this node, update transitivity
     * p. 10, 2.1.1, equation 3
     */
    void update_transitive(double ab, double bc);

    /**
     * p. 9, 2.1.1, equation 2
     */
    void update_age();

    bool set_encounter( double d ) {
        if ( d >= 0.0 && d <= 1.0 ) { params_->encounter_ = d; return true; }
        return false;
    }
    bool set_beta( double d ) {
        if ( d >= 0.0 && d <= 1.0 ) { params_->beta_ = d; return true; }
        return false;
    }
    bool set_gamma( double d ) {
        if ( d >= 0.0 && d <= 1.0 ) { params_->gamma_ = d; return true; }
        return false;
    }
    void set_age_factor(u_int ms_per_unit) {
        params_->kappa_ = ms_per_unit;
    }
    
protected:
    /**
     * Use kappa_ milliseconds per unit to convert t (as a diff)
     * to time units for use in Eq. 2 
     */
    u_int  time_to_units(oasys::Time diff);

    ProphetNodeParams* params_; ///< initialization constants
    double p_value_;   ///< Delivery prediction for remote
    bool   relay_;     ///< Relay node
    bool   custody_;   ///< Custody node
    bool   internet_gateway_; ///< Internet gateway node
    EndpointIDPattern remote_eid_; ///< implicit DTN route to remote
    oasys::Time age_;  ///< time of last write to pvalue
}; // ProphetNode

class RIBNode : public ProphetNode
{
public:
    RIBNode(ProphetNode* node, u_int16_t sid)
        : ProphetNode(*node), sid_(sid)
    {}
    RIBNode(u_int16_t sid = 0)
        : ProphetNode(), sid_(sid)
    {}
    RIBNode(const RIBNode& a)
        : ProphetNode(a), sid_(a.sid_)
    {}
    virtual ~RIBNode()
    {}
    RIBNode& operator= (const RIBNode& a) {
        ProphetNode::operator=(a);
        sid_ = a.sid_;
        return *this;
    }
    u_int16_t sid_;
}; // RIBNode

#define BUNDLE_OFFER_LOGPATH "/dtn/route/prophet/bundleoffer"
/**
 * BundleOffer represents an entry from a BundleOfferTLV
 */
class BundleOffer : public oasys::Logger
{
public:
    typedef enum {
        UNDEFINED = 0,
        OFFER,
        RESPONSE
    } bundle_offer_t;

    // set true for Offer, false for Response
    BundleOffer(bundle_offer_t type = UNDEFINED)
        : oasys::Logger("BundleOffer",BUNDLE_OFFER_LOGPATH),
          cts_(0), sid_(0),
          custody_(false), accept_(false), ack_(false),
          type_(type)
    {}

    BundleOffer(const BundleOffer& b) 
        : oasys::Logger("BundleOffer",BUNDLE_OFFER_LOGPATH),
          cts_(b.cts_), sid_(b.sid_), custody_(b.custody_),
          accept_(b.accept_), ack_(b.ack_),
          type_(b.type_)
    {}

    BundleOffer(u_int32_t cts, u_int16_t sid, bool custody=false,
                bool accept=false, bool ack=false,
                bundle_offer_t type=UNDEFINED)
        : oasys::Logger("BundleOffer",BUNDLE_OFFER_LOGPATH),
          cts_(cts), sid_(sid),
          custody_(custody), accept_(accept), ack_(ack),
          type_(type)
    {}

    BundleOffer& operator= (const BundleOffer& b) {
        cts_     = b.cts_;
        sid_     = b.sid_;
        custody_ = b.custody_;
        accept_  = b.accept_;
        ack_     = b.ack_;
        type_    = b.type_;
        return *this;
    }

    bool operator< (const BundleOffer& b) const {
        if (sid_ == b.sid_)
            return (cts_ < b.cts_);
        return (sid_ < b.sid_);
    }

    u_int32_t creation_ts() const { return cts_; }
    u_int16_t sid() const { return sid_; }
    bool custody() const { return custody_; }
    bool accept() const { return accept_; }
    bool ack() const { return ack_; }
    bundle_offer_t type() const { return type_; }

protected:

    u_int32_t cts_; ///< Creation timestamp
    u_int16_t sid_; ///< string id of bundle destination
    bool      custody_;
    bool      accept_;
    bool      ack_;
    bundle_offer_t type_; ///< indicates whether offer or response TLV

}; // BundleOffer

class ProphetAck {
public:
    ProphetAck();
    ProphetAck(const EndpointID& eid,u_int32_t cts=0,u_int32_t ets=0);
    ProphetAck(const ProphetAck&);

    ProphetAck& operator= (const ProphetAck&);
    bool operator< (const ProphetAck&) const;

    EndpointID dest_id_; ///< destination EndpointID
    u_int32_t  cts_;     ///< creation timestamp
    u_int32_t  ets_;     ///< expiration timestamp
}; // ProphetAck

}; // dtn

#endif // _DTN_PROPHET_NODE_

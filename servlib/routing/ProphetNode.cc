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

#include "ProphetNode.h"
#include <netinet/in.h> // for ntoh*,hton*
#include <math.h> // for pow()

namespace dtn {

ProphetNode::ProphetNode(
        ProphetNodeParams* params,
        const char* logbase)
    : oasys::Logger("ProphetNode",logbase),
      params_(params), p_value_(0.0), relay_(true),
      custody_(true), internet_gateway_(false),
      remote_eid_(EndpointID::NULL_EID())
{
    age_.get_time(); // fill with current time
}

ProphetNode::ProphetNode(const ProphetNode& n)
    : oasys::Logger("ProphetNode",
                    ((oasys::Logger)n).logpath()),
      oasys::Formatter(),
      oasys::SerializableObject(),
      params_(n.params_),
      p_value_(n.p_value_), relay_(n.relay_), custody_(n.custody_),
      internet_gateway_(n.internet_gateway_), remote_eid_(n.remote_eid_)
{
}

ProphetNode::ProphetNode(const oasys::Builder&)
    : oasys::Logger("ProphetNode","/dtn/route/prophet"),
      params_(NULL), p_value_(0.0), relay_(false), custody_(false),
      internet_gateway_(false), remote_eid_(EndpointID::NULL_EID())
{
}

int
ProphetNode::format(char* buf, size_t sz) const
{
    oasys::Time t = age_;
    return snprintf(buf, sz, "prophet node %s %0.4f (%s%s%s last mod %u ms ago)",
                    remote_eid_.c_str(), p_value_,
                    relay_ ? "relay " : "",
                    custody_ ? "custody " : "",
                    internet_gateway_ ? "internet gw " : "",
                    t.elapsed_ms());
}

void
ProphetNode::format_verbose(oasys::StringBuffer* buf)
{
    dump(buf);
}

void
ProphetNode::serialize(oasys::SerializeAction* a)
{
    a->process("remote_eid",&remote_eid_);
    //XXX/wilson This stinks, should be a double
    a->process("p_value",(u_int64_t*)&p_value_);
    a->process("age",&age_.sec_);
    a->process("custody",&custody_);
    a->process("relay",&relay_);
    a->process("internet_gw",&internet_gateway_);
}

void
RIBNode::dump(oasys::StringBuffer* buf)
{
    buf->appendf("\tSID %d\n",sid_);
    ProphetNode::dump(buf);
}

void
ProphetNode::dump(oasys::StringBuffer* buf)
{
    buf->appendf("\tEndpointID %s\n"
                 "\tP Value %0.4f\n"
                 "\tRelay %s\n"
                 "\tCustody %s\n"
                 "\tInternet GW %s\n"
                 "\tLast mod %d ms ago\n",
                 remote_eid_.c_str(),
                 p_value_,
                 relay_ ? "true" : "false",
                 custody_ ? "true" : "false",
                 internet_gateway_ ? "true" : "false",
                 age_.elapsed_ms());
}

void
BundleOffer::dump(oasys::StringBuffer* buf)
{
    buf->appendf("\t%s\n"
                 "\tCreation TS %d\n"
                 "\tSeqNo %d\n"
                 "\tSID %d\n"
                 "\tCustody %s\n"
                 "\tAccept %s\n"
                 "\tAck %s\n",
                 type_to_str(type_),
                 cts_, seq_, sid_,
                 custody_ ? "true" : "false",
                 accept_ ? "true" : "false",
                 ack_ ? "true" : "false");
}

bool
ProphetNode::route_to_me(const EndpointID& eid) const
{
    return Prophet::eid_to_route(remote_eid_).match(eid);
}

/**
 * Equation 1, Section 2.1.1
 * P_(A,B) = P_(A,B)_old + ( 1 - P_(A,B)_old ) * P_encounter
 */
void
ProphetNode::update_pvalue()
{
    // new p_value is P_(A,B), previous p_value_ is P_(A,B)_old
    // params_->encounter_ is P_encounter
    // A is the local node
    // B is the node just encountered, represented by this ProphetNode instance
    ASSERT(p_value_ >= 0.0 && p_value_ <= 1.0);
    double prev = p_value_;
    (void)prev;
    p_value_ = p_value_ + (1.0 - p_value_) * params_->encounter_;
    log_debug("update_pvalue: before %.2f after %.2f",prev,p_value_);
    // update age to reflect data "freshness"
    age_.get_time();
}

/**
 * Equation 3, Section 2.1.1
 * P_(A,C) = P_(A,C)_old + ( 1 - P_(A,C)_old ) * P_(A,B) * P_(B,C) * beta
 */
void
ProphetNode::update_transitive(double ab, double bc)
{
    // new p_value_ is P_(A,C), previous p_value is P_(A,C)_old
    // params_->beta_ is beta
    // A is the local node
    // B is the node just encountered (originator of RIB)
    // C is the node represented by this ProphetNode instance
    ASSERT(p_value_ >= 0.0 && p_value_ <= 1.0);
    ASSERT(ab >= 0.0 && ab <= 1.0);
    ASSERT(bc >= 0.0 && bc <= 1.0);
    double prev = p_value_;
    (void)prev;
    p_value_ = p_value_ + (1.0 - p_value_) * ab * bc * params_->beta_;
    log_debug("update_transitive: ab %.2f bc %.2f before %.2f after %.2f",
              ab,bc,prev,p_value_);
    // update age to reflect data "freshness"
    age_.get_time();
}

/**
 * Equation 2, Section 2.1.1
 * P_(A,B) = P_(A,B)_old * gamma^K
 */
void
ProphetNode::update_age()
{
    // new p_value_ is P_(A,B), previous p_value is P_(A,B)_old
    // params_->gamma_ is gamma
    // timeunits is k
    ASSERT(p_value_ >= 0.0 && p_value_ <= 1.0);
    double gfactor = params_->gamma_;

    oasys::Time now;
    now.get_time();
    u_int timeunits = time_to_units(now - age_);
    age_.get_time();
    
    if ( timeunits == 0 )
        gfactor = 1;
    else if ( timeunits > 1 )
        gfactor = pow( params_->gamma_, timeunits );
    double prev = p_value_;
    (void)prev;
    p_value_ *= gfactor;
    log_debug("update_age: timeunits %u before %.2f after %.2f",
              timeunits,prev,p_value_);
}

u_int
ProphetNode::time_to_units(oasys::Time diff)
{
    // params_->kappa_ is in units of milliseconds per timeunit
    ASSERT(params_->kappa_ > 0.0);
    u_int retval = (u_int) diff.sec_ * 1000 / params_->kappa_;
    retval += (u_int) diff.usec_ / (1000 * params_->kappa_);
    log_debug("time_to_units: diff sec %u usec %u timeunits %u",
              diff.sec_,diff.usec_,retval);
    return retval;
}

ProphetAck::ProphetAck()
    : dest_id_(EndpointID::NULL_EID()),
      cts_(0), seq_(0), ets_(0)
{
}

ProphetAck::ProphetAck(const EndpointID& eid,
                       u_int32_t cts,
                       u_int32_t seq,
                       u_int32_t ets)
    : dest_id_(eid), cts_(cts), seq_(seq), ets_(ets)
{
}

ProphetAck::ProphetAck(const ProphetAck& p)
    : dest_id_(p.dest_id_), cts_(p.cts_), seq_(p.seq_), ets_(p.ets_)
{
    ASSERT(!dest_id_.equals(EndpointID::NULL_EID()));
}

bool
ProphetAck::operator<(const ProphetAck& p) const
{
    if(dest_id_.equals(p.dest_id_))
    {
        if (cts_ == p.cts_)
            return seq_ < p.seq_;
        else
            return (cts_ < p.cts_);
    }
    return (dest_id_.str() < p.dest_id_.str());
}

ProphetAck&
ProphetAck::operator=(const ProphetAck& p) 
{
    dest_id_ = p.dest_id_;
    cts_ = p.cts_;
    seq_ = p.seq_;
    ets_ = p.ets_;
    return *this;
}

} // namespace dtn

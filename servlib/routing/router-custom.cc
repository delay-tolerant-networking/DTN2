/*
 *    Copyright 2006-2007 The MITRE Corporation
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
 *
 *    The US Government will not be charged any license fee and/or royalties
 *    related to this software. Neither name of The MITRE Corporation; nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include "router-custom.h"
#include <conv_layers/TCPConvergenceLayer.h>
#include <conv_layers/UDPConvergenceLayer.h>
#include <conv_layers/BluetoothConvergenceLayer.h>
#include <oasys/io/NetUtils.h>

namespace dtn {
namespace rtrmessage {

std::string
lowercase(const char *c_str)
{
    std::string str(c_str);
    transform (str.begin(), str.end(), str.begin(), to_lower());
    return str;
}

// linkType

linkType::linkType(const remote_eid::type& a,
              const type::type_& b,
              const nexthop::type& c,
              const state::type& d,
              const is_reachable::type& e,
              const is_usable::type& f,
              const how_reliable::type& g,
              const how_available::type& h,
              const clayer::type& i,
              const min_retry_interval::type& j,
              const max_retry_interval::type& k,
              const idle_close_time::type& l)
    : linkType_base(a, b, c, d, e, f, g, h, i, j, k, l)
{
}
  
linkType::linkType(const ::xercesc::DOMElement& a,
              ::xml_schema::flags b,
              ::xml_schema::type* c)
    : linkType_base(a, b, c)
{
}

linkType::linkType(const linkType& a,
              ::xml_schema::flags b,
              ::xml_schema::type* c)
    : linkType_base(a, b, c)
{
}

linkType::linkType(Link* l)
    : linkType_base(
        eidType(l->remote_eid().str()),
        linkTypeType(lowercase(l->type_str())),
        l->nexthop(),
        (xml_schema::string)lowercase(l->state_to_str(l->state())),
        true, // @@@ is_reachable (where do we get this from?)
        l->is_usable(),
        l->stats()->reliability_,
        l->stats()->availability_,
        l->clayer()->name(),
        l->params().min_retry_interval_,
        l->params().max_retry_interval_,
        l->params().idle_close_time_)
{
    if (l->cl_info()) {

        // We have access to convergence layer settings
        typedef linkType::clinfo::type clinfo_t;
        std::auto_ptr<clinfo_t> info (new clinfo_t());

        #define STREAM_PARAMS \
            info->segment_ack_enabled(params->segment_ack_enabled_); \
            info->negative_ack_enabled(params->negative_ack_enabled_); \
            info->keepalive_interval(params->keepalive_interval_); \
            info->segment_length(params->segment_length_);

        #define LINK_PARAMS \
            info->reactive_frag_enabled(params->reactive_frag_enabled_); \
            info->sendbuf_length(params->sendbuf_len_); \
            info->recvbuf_length(params->recvbuf_len_); \
            info->data_timeout(params->data_timeout_);

        if (clayer().compare("tcp") == 0) {
            typedef TCPConvergenceLayer::TCPLinkParams tcp_params;
            tcp_params *params = dynamic_cast<tcp_params*>(l->cl_info());
            if(params == 0) return;

            oasys::Intoa local_addr(params->local_addr_);
            info->local_addr(local_addr.buf());
            oasys::Intoa remote_addr(params->remote_addr_);
            info->remote_addr(remote_addr.buf());
            info->remote_port(params->remote_port_);

            STREAM_PARAMS
            LINK_PARAMS
        }

        if (clayer().compare("udp") == 0) {
            typedef UDPConvergenceLayer::Params udp_params;
            udp_params *params = dynamic_cast<udp_params*>(l->cl_info());
            if(params == 0) return;

            oasys::Intoa local_addr(params->local_addr_);
            info->local_addr(local_addr.buf());
            oasys::Intoa remote_addr(params->remote_addr_);
            info->remote_addr(remote_addr.buf());
            info->local_port(params->local_port_);
            info->remote_port(params->remote_port_);
            info->rate(params->rate_);
            info->bucket_depth(params->bucket_depth_);
        }

#ifdef OASYS_BLUETOOTH_ENABLED
        if (clayer().compare("bt") == 0) {
            typedef BluetoothConvergenceLayer::BluetoothLinkParams bt_params;
            bt_params *params = dynamic_cast<bt_params*>(l->cl_info());
            if(params == 0) return;

            info->local_addr(bd2str(params->local_addr_));
            info->remote_addr(bd2str(params->remote_addr_));
            info->channel(params->channel_);

            STREAM_PARAMS
            LINK_PARAMS
        }
#endif

        clinfo(info);
    }
}

linkType*
linkType::_clone (::xml_schema::flags a,
                  ::xml_schema::type* b) const
{
    return new linkType(*this, a, b);
}


// bundleType

bundleType::bundleType (const source::type& a,
                        const dest::type& b,
                        const custodian::type& c,
                        const replyto::type& d,
                        const prevhop::type& e,
                        const length::type& f,
                        const location::type& g,
                        const bundleid::type& h,
                        const is_fragment::type& i,
                        const is_admin::type& j,
                        const do_not_fragment::type& k,
                        const priority::type& l,
                        const custody_requested::type& m,
                        const local_custody::type& n,
                        const singleton_dest::type& o,
                        const custody_rcpt::type& p,
                        const receive_rcpt::type& q,
                        const forward_rcpt::type& r,
                        const delivery_rcpt::type& s,
                        const deletion_rcpt::type& t,
                        const app_acked_rcpt::type& u,
                        const creation_ts_seconds::type& v,
                        const creation_ts_seqno::type& w,
                        const expiration::type& x,
                        const orig_length::type& y,
                        const frag_offset::type& z,
                        const owner::type& aa)
    : bundleType_base (a, b, c, d, e, f, g, h, i,
                       j, k, l, m, n, o, p, q, r,
                       s, t, u, v, w, x, y, z, aa)
{
}

bundleType::bundleType (const ::xercesc::DOMElement& a,
                        ::xml_schema::flags b,
                        ::xml_schema::type* c)
    : bundleType_base(a, b, c)
{
}

bundleType::bundleType (const bundleType& a,
                        ::xml_schema::flags b,
                        ::xml_schema::type* c)
    : bundleType_base(a, b, c)
{
}

bundleType::bundleType (Bundle* b)
    : bundleType_base(
        eidType(b->source_.str()),
        eidType(b->dest_.str()),
        eidType(b->custodian_.str()),
        eidType(b->replyto_.str()),
        eidType(b->prevhop_.str()),
        b->payload_.length(),
        bundleLocationType(location_to_str(b->payload_.location())),
        b->bundleid_,
        b->is_fragment_,
        b->is_admin_,
        b->do_not_fragment_,
        bundlePriorityType(lowercase(b->prioritytoa(b->priority_))),
        b->custody_requested_,
        b->local_custody_,
        b->singleton_dest_,
        b->custody_rcpt_,
        b->receive_rcpt_,
        b->forward_rcpt_,
        b->delivery_rcpt_,
        b->deletion_rcpt_,
        b->app_acked_rcpt_,
        b->creation_ts_.seconds_,
        b->creation_ts_.seqno_,
        b->expiration_,
        b->orig_length_,
        b->frag_offset_,
        b->owner_)
{
}

bundleType*
bundleType::_clone (::xml_schema::flags a,
                    ::xml_schema::type* b) const
{
    return new bundleType(*this, a, b);
}

const char *
bundleType::location_to_str(int location)
{
    switch(location) {
        case BundlePayload::MEMORY: return "memory";
        case BundlePayload::DISK:   return "disk";
        case BundlePayload::NODATA: return "nodata";
        default: return "";
    }
}

// contactType

contactType::contactType (const link_attr::type& a,
                          const start_time_sec::type& b,
                          const start_time_usec::type& c,
                          const duration::type& d,
                          const bps::type& e,
                          const latency::type& f,
                          const pkt_loss_prob::type& g)
    : contactType_base (a, b, c, d, e, f, g)
{
}

contactType::contactType (const ::xercesc::DOMElement& a,
                          ::xml_schema::flags b,
                          ::xml_schema::type* c)
    : contactType_base (a, b, c)
{
}

contactType::contactType (const contactType& a,
                          ::xml_schema::flags b,
                          ::xml_schema::type* c)
    : contactType_base (a, b, c) 
{
}

contactType::contactType (Contact* c)
    : contactType_base (
        linkType(c->link().object()),
        c->start_time().sec_,
        c->start_time().usec_,
        c->duration(),
        c->bps(),
        c->latency(),
        0)  // @@@ pkt_loss_prob (where do we get this from?)
{
}

contactType*
contactType::_clone (::xml_schema::flags a,
                     ::xml_schema::type* b) const
{
    return new contactType(*this, a, b);
}


// eidType

eidType::eidType (const uri::type& a)
    : eidType_base (a)
{
}

eidType::eidType (const ::xercesc::DOMElement& a,
                        ::xml_schema::flags b,
                        ::xml_schema::type* c)
    : eidType_base(a, b, c)
{
}

eidType::eidType (const eidType& a,
                        ::xml_schema::flags b,
                        ::xml_schema::type* c)
    : eidType_base(a, b, c)
{
}

eidType::eidType (dtn::EndpointID& e)
    : eidType_base (e.str())
{
}

eidType::eidType (std::string& a)
    : eidType_base (a)
{
}

eidType*
eidType::_clone (::xml_schema::flags a,
                    ::xml_schema::type* b) const
{
    return new eidType(*this, a, b);
}

// gbofIdType

gbofIdType::gbofIdType (const source::type& a,
                        const creation_ts::type& b,
                        const is_fragment::type& c,
                        const frag_length::type& d,
                        const frag_offset::type& e)
    : gbofIdType_base (a, b, c, d, e)
{
}

gbofIdType::gbofIdType (const ::xercesc::DOMElement& a,
                        ::xml_schema::flags b,
                        ::xml_schema::type* c)
    : gbofIdType_base(a, b, c)
{
}

gbofIdType::gbofIdType (const gbofIdType& a,
                        ::xml_schema::flags b,
                        ::xml_schema::type* c)
    : gbofIdType_base(a, b, c)
{
}

gbofIdType::gbofIdType (dtn::Bundle* b)
    : gbofIdType_base (
        eidType(b->source_.str()),
        bundle_ts_to_long(b->creation_ts_),
        b->is_fragment_,
        b->is_fragment_ ? b->payload_.length() : 0,
        b->is_fragment_ ? b->frag_offset_ : 0)
{
}

gbofIdType::gbofIdType (dtn::CustodySignal::data_t d)
    : gbofIdType_base (
        eidType(d.orig_source_eid_.str()),
        bundle_ts_to_long(d.orig_creation_tv_),
        d.admin_flags_ & BundleProtocol::ADMIN_IS_FRAGMENT,
        d.admin_flags_ & BundleProtocol::ADMIN_IS_FRAGMENT ? d.orig_frag_length_ : 0,
        d.admin_flags_ & BundleProtocol::ADMIN_IS_FRAGMENT ? d.orig_frag_offset_ : 0)
{
}

gbofIdType*
gbofIdType::_clone (::xml_schema::flags a,
                    ::xml_schema::type* b) const
{
    return new gbofIdType(*this, a, b);
}

// key_value_pair

key_value_pair::key_value_pair (const name::type& a,
                                const bool_value::type& b)
    : key_value_pair_base (a)
{
  bool_value(b);
}

key_value_pair::key_value_pair (const name::type& a,
                                const u_int_value::type& b)
    : key_value_pair_base (a)
{
  u_int_value(b);
}

key_value_pair::key_value_pair (const name::type& a,
                                const int_value::type& b)
    : key_value_pair_base (a)
{
  int_value(b);
}

key_value_pair::key_value_pair (const name::type& a,
                                const str_value::type& b)
    : key_value_pair_base (a)
{
  str_value(b);
}

key_value_pair::key_value_pair (const ::xercesc::DOMElement& a,
                                ::xml_schema::flags b,
                                ::xml_schema::type* c)
    : key_value_pair_base(a, b, c)
{
}

key_value_pair::key_value_pair (const key_value_pair& a,
                                ::xml_schema::flags b,
                                ::xml_schema::type* c)
    : key_value_pair_base(a, b, c)
{
}

key_value_pair::key_value_pair (const dtn::NamedAttribute& na)
    : key_value_pair_base (na.name())
{
    switch(na.type()) {
        case NamedAttribute::ATTRIBUTE_TYPE_INVALID:
            // XXX wish I could log warning, logging not available here
            break;

        case NamedAttribute::ATTRIBUTE_TYPE_INTEGER:
            int_value(na.int_val());
            break;

        case NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INTEGER:
            u_int_value(na.u_int_val());
            break;

        case NamedAttribute::ATTRIBUTE_TYPE_BOOLEAN:
            bool_value(na.bool_val());
            break;

        case NamedAttribute::ATTRIBUTE_TYPE_STRING:
            str_value(na.string_val());
            break;

        default:
            // XXX wish I could log warning, logging not available here
            break;
        }
}

key_value_pair*
key_value_pair::_clone (::xml_schema::flags a,
                    ::xml_schema::type* b) const
{
    return new key_value_pair(*this, a, b);
}

// routeEntryType

routeEntryType::routeEntryType (const dest_pattern::type& a,
                                const source_pattern::type& b,
                                const route_priority::type& c,
                                const action::type& d,
                                const link::type& e)
    : routeEntryType_base (a, b, c, d, e)
{
}

routeEntryType::routeEntryType (const ::xercesc::DOMElement& a,
                                ::xml_schema::flags b,
                                ::xml_schema::type* c)
    : routeEntryType_base (a, b, c)
{
}

routeEntryType::routeEntryType (const routeEntryType& a,
                                ::xml_schema::flags b,
                                ::xml_schema::type* c)
    : routeEntryType_base (a, b, c)
{
}

routeEntryType::routeEntryType (RouteEntry* e)
    : routeEntryType_base (
        eidType(e->dest_pattern().str()),
        eidType(e->source_pattern().str()),
        e->priority(),
        bundleForwardActionType(
            lowercase(ForwardingInfo::action_to_str(e->action()))),
        e->link()->name_str())
{
}

routeEntryType*
routeEntryType::_clone (::xml_schema::flags a,
                        ::xml_schema::type* b) const
{
    return new routeEntryType(*this, a, b);
}


// registrationType


registrationType::registrationType (const endpoint::type&,
                                    const regid::type&,
                                    const action::type&,
                                    const script::type&,
                                    const expiration::type&)
{
}

registrationType::registrationType (const ::xercesc::DOMElement& a,
                                    ::xml_schema::flags b,
                                    ::xml_schema::type* c)
    : registrationType_base (a, b, c)
{
}

registrationType::registrationType (const registrationType& a,
                                    ::xml_schema::flags b,
                                    ::xml_schema::type* c)
    : registrationType_base (a, b, c)
{
}

registrationType::registrationType (Registration* r)
    : registrationType_base (
        eidType(r->endpoint().str()),
        r->regid(),
        failureActionType(lowercase(r->failure_action_toa(r->failure_action()))),
        r->script(),
        r->expiration())
{
}

registrationType*
registrationType::_clone (::xml_schema::flags a,
                          ::xml_schema::type* b) const
{
    return new registrationType(*this, a, b);
}

} // namespace rtrmessage
} // namespace dtn

#endif // XERCES_C_ENABLED && EXTERNAL_DP_ENABLED

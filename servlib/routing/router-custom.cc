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

#include <config.h>
#ifdef XERCES_C_ENABLED

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
              const name::type& d,
              const state::type& e,
              const reliable::type& f,
              const clayer::type& g,
              const min_retry_interval::type& h,
              const max_retry_interval::type& i,
              const idle_close_time::type& j)
    : linkType_base(a, b, c, d, e, f, g, h, i, j)
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
        l->name(),
        (xml_schema::string)lowercase(l->state_to_str(l->state())),
        l->is_reliable(),
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
            info->busy_queue_depth(params->busy_queue_depth_); \
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

contactType::contactType (const link::type& a,
                          const start_time_sec::type& b,
                          const start_time_usec::type& c,
                          const duration::type& d,
                          const bps::type& e,
                          const latency::type& f)
    : contactType_base (a, b, c, d, e, f)
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
        c->start_time_.tv_sec,
        c->start_time_.tv_usec,
        c->duration_ms_,
        c->bps_,
        c->latency_ms_)
{
}

contactType*
contactType::_clone (::xml_schema::flags a,
                     ::xml_schema::type* b) const
{
    return new contactType(*this, a, b);
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
        eidType(e->dest_pattern_.str()),
        eidType(e->source_pattern_.str()),
        e->route_priority_,
        bundleForwardActionType(lowercase(
            ForwardingInfo::action_to_str((ForwardingInfo::action_t)(e->action_)))),
        e->next_hop_->name_str())
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

#endif // XERCES_C_ENABLED

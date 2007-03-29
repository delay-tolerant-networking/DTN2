/*
 *    Copyright 2007 Intel Corporation
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <oasys/util/ScratchBuffer.h>

#include "ShortestPathRouter.h"
#include "bundling/BundleDaemon.h"
#include "bundling/SDNV.h"
#include "bundling/TempBundle.h"
#include "contacts/ContactManager.h"

namespace oasys {

//----------------------------------------------------------------------
template<>
const char*
InlineFormatter<dtn::ShortestPathRouter::EdgeInfo>
  ::format(const dtn::ShortestPathRouter::EdgeInfo& ei)
{
    buf_.appendf("%s: cost=%u delay=%u bw=%u",
                 ei.id_.c_str(), ei.params_.cost_,
                 ei.params_.delay_, ei.params_.bw_);
    return buf_.c_str();
}

} // namespace oasys

namespace dtn {

//----------------------------------------------------------------------
ShortestPathRouter::ShortestPathRouter()
    : BundleRouter("ShortestPathRouter", "shortest_path"),
      announce_eid_("dtn://*/lsa"),
      forward_pending_("router forward_pending"),
      dupcache_(1000) // XXX/demmer parameterize cache size
{
    weight_fn_ = new CostWeightFn();
}

//----------------------------------------------------------------------
void
ShortestPathRouter::handle_event(BundleEvent* event)
{
    dispatch_event(event);
}

//----------------------------------------------------------------------
void
ShortestPathRouter::initialize()
{
    const EndpointID& local_eid = BundleDaemon::instance()->local_eid();
    log_debug("initializing local_eid %s", local_eid.c_str());
    local_node_ = graph_.add_node(local_eid.str(), NodeInfo());

    EndpointIDPattern admin_eid = local_eid;
    admin_eid.append_service_tag("lsa");
    reg_ = new SPReg(this, admin_eid);
}

//----------------------------------------------------------------------
void
ShortestPathRouter::get_routing_state(oasys::StringBuffer* buf)
{
    buf->appendf("Shortest Path Routing Graph:\n*%p", &graph_);
}

//----------------------------------------------------------------------
void
ShortestPathRouter::handle_bundle_received(BundleReceivedEvent* e)
{
    Bundle* bundle = e->bundleref_.object();
    log_debug("handle bundle received: *%p", bundle);

    if (dupcache_.is_duplicate(bundle)) {
        log_debug("ignoring duplicate bundle");
        return;
    }
    
    try_to_forward_bundle(bundle);
}

//----------------------------------------------------------------------
void
ShortestPathRouter::handle_link_created(LinkCreatedEvent* e)
{
    const LinkRef& link = e->link_;
    if (link->remote_eid() == EndpointID::NULL_EID()) {
        log_warn("can't handle link %s: no remote endpoint id",
                 link->name());
        return;
    }

    RoutingGraph::Node* remote_node =
        graph_.find_node(link->remote_eid().str());
    if (remote_node == NULL) {
        remote_node = graph_.add_node(link->remote_eid().str(), NodeInfo());
    }

    EdgeInfo ei(link->name());
    ei.params_.cost_ = link->params().cost_;
    RoutingGraph::Edge* edge = graph_.add_edge(local_node_, remote_node, ei);
    
    // announce the new edge to all peers
    broadcast_announcement(edge);
    
    // dump the routing graph state to the newly added link
    dump_state(link);
}

//----------------------------------------------------------------------
void
ShortestPathRouter::handle_link_deleted(LinkDeletedEvent* e)
{
    RoutingGraph::EdgeVector::iterator iter;
    for (iter = local_node_->out_edges_.begin();
         iter != local_node_->out_edges_.end(); ++iter)
    {
        if ((*iter)->info_.id_ == e->link_->name()) {
            bool ok = graph_.del_edge(local_node_, *iter);
            ASSERT(ok);
            return;
        }
    }

    log_err("handle_link_deleted: can't find link *%p", e->link_.object());
}

//----------------------------------------------------------------------
void
ShortestPathRouter::try_to_forward_bundle(Bundle* b)
{
    log_debug("try_to_forward_bundle(*%p)", b);
    if (b->singleton_dest_) {
        unicast_bundle(b);
    } else {
        multicast_bundle(b);
    }
}

//----------------------------------------------------------------------
void
ShortestPathRouter::unicast_bundle(Bundle* b)
{
    log_debug("unicast_bundle(*%p)", b);

    // XXX/demmer do the shortest path check
}


//----------------------------------------------------------------------
void
ShortestPathRouter::multicast_bundle(Bundle* b)
{
    // XXX/demmer need RPF check, need group membership check -- this
    // just floods the bundle everywhere, relying on the duplicate
    // cache to avoid infinite floods
    
    RoutingGraph::EdgeVector::iterator ei;
    RoutingGraph::EdgeVector::iterator ei2;
    
    RoutingGraph::EdgeVector best_edges;

    for (ei = local_node_->out_edges_.begin();
         ei != local_node_->out_edges_.end(); ++ei)
    {
        bool add_edge = true;
        for (ei2 = best_edges.begin(); ei2 != best_edges.end(); ++ei2)
        {
            // check if there's already an edge to this destination,
            // if so, we need to check its weight and potentially
            // remove it, to be replaced with the new one
            if ((*ei)->dest_ == (*ei2)->dest_)
            {
                if ((*weight_fn_)(*ei) < (*weight_fn_)(*ei2)) {
                    best_edges.erase(ei2);
                } else {
                    add_edge = false;
                }
                break;
            }
        }

        if (add_edge) {
            best_edges.push_back(*ei);
        }
    }

    log_debug("multicast_bundle(*%p): found %zu edges", b, best_edges.size());

    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    for (ei = best_edges.begin(); ei != best_edges.end(); ++ei) {
        LinkRef link = cm->find_link((*ei)->info_.id_.c_str());
        if (link == NULL) {
            log_crit("internal error: link %s not in local link table!!",
                     (*ei)->info_.id_.c_str());
            continue;
        }

        if (link->isavailable() && (!link->isopen()) && (!link->isopening())) {
            log_debug("opening *%p because a message is intended for it",
                      link.object());
            actions_->open_link(link);
        }
        
        log_debug("multicast_bundle: sending bundle to link *%p", link.object());
        actions_->send_bundle(b, link, ForwardingInfo::COPY_ACTION,
                              CustodyTimerSpec());
    }
}

//----------------------------------------------------------------------
u_int32_t
ShortestPathRouter::CostWeightFn::operator()(RoutingGraph::Edge* edge)
{
    return edge->info_.params_.cost_;
}

//----------------------------------------------------------------------
ShortestPathRouter::SPReg::SPReg(ShortestPathRouter* router,
                                 const EndpointIDPattern& eid)
    : Registration(SHORTESTPATHROUTER_REGID, eid,
                   Registration::DEFER, 0),
      router_(router)
{
    logpathf("%s/reg", router->logpath()),
    BundleDaemon::post(new RegistrationAddedEvent(this, EVENTSRC_ADMIN));
}

//----------------------------------------------------------------------
void
ShortestPathRouter::SPReg::deliver_bundle(Bundle* bundle)
{
    log_notice("got bundle *%p", bundle);
    router_->handle_lsa_bundle(bundle);
    BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
}

//----------------------------------------------------------------------
void
ShortestPathRouter::handle_lsa_bundle(Bundle* bundle)
{
    LSAVector lsa_vec;
    if (!parse_lsa_bundle(bundle, &lsa_vec)) {
        log_warn("error parsing LSA");
    }

    for (LSAVector::iterator i = lsa_vec.begin(); i != lsa_vec.end(); ++i)
    {
        LSA* lsa = &(*i);

        RoutingGraph::Node *a, *b;

        a = graph_.find_node(lsa->source_.str());
        if (!a) {
            log_debug("handle_lsa_bundle: adding new source node %s",
                      lsa->source_.c_str());
            a = graph_.add_node(lsa->source_.str(), NodeInfo());
        }

        b = graph_.find_node(lsa->dest_.str());
        if (!b) {
            log_debug("handle_lsa_bundle: adding new dest node %s",
                      lsa->dest_.c_str());
            b = graph_.add_node(lsa->dest_.str(), NodeInfo());
        }

        RoutingGraph::Edge *e = NULL;
        RoutingGraph::EdgeVector::iterator ei;
        for (ei = a->out_edges_.begin(); ei != a->out_edges_.end(); ++ei) {
            if ((*ei)->info_.id_ == lsa->id_) {
                log_debug("handle_lsa_bundle: found edge %s from %s -> %s",
                          lsa->id_.c_str(), a->id_.c_str(), b->id_.c_str());
                e = *ei;
                break;
            }
        }

        if (e == NULL) {
            e = graph_.add_edge(a, b, EdgeInfo(lsa->id_, lsa->params_));
            log_debug("handle_lsa: adding new edge %s from %s -> %s",
                      lsa->id_.c_str(), a->id_.c_str(), b->id_.c_str());
        }

        log_debug("handle_lsa: updating edge %s from %s -> %s",
                  lsa->id_.c_str(), a->id_.c_str(), b->id_.c_str());
                  
        e->info_.params_ = lsa->params_;

        // XXX/demmer should add expiration timer for the edge
    }
}

//----------------------------------------------------------------------
void
ShortestPathRouter::format_lsa_bundle(Bundle* bundle, LSAVector* lsa_vec)
{
    oasys::ScratchBuffer<u_char*, 256> buf;

    size_t len;
    size_t sdnv_len;
    
    // first serialize the size of the vector
    len = lsa_vec->size();
    sdnv_len = SDNV::encode(len, buf.tail_buf(SDNV::MAX_LENGTH),
                            SDNV::MAX_LENGTH);
    buf.incr_len(sdnv_len);

    LSAVector::const_iterator i;
    for (i = lsa_vec->begin(); i != lsa_vec->end(); ++i)
    {
        const LSA* lsa = &(*i);

        // the source eid
        len = lsa->source_.length();
        sdnv_len = SDNV::encode(len, buf.tail_buf(SDNV::MAX_LENGTH),
                                SDNV::MAX_LENGTH);
        buf.incr_len(sdnv_len);
        memcpy(buf.tail_buf(len), lsa->source_.data(), len);
        buf.incr_len(len);
        
        // the dest eid
        len = lsa->dest_.length();
        sdnv_len = SDNV::encode(len, buf.tail_buf(SDNV::MAX_LENGTH),
                                SDNV::MAX_LENGTH);
        buf.incr_len(sdnv_len);
        memcpy(buf.tail_buf(len), lsa->dest_.data(), len);
        buf.incr_len(len);
         
        // the link id
        len = lsa->id_.length();
        sdnv_len = SDNV::encode(len, buf.tail_buf(SDNV::MAX_LENGTH),
                                SDNV::MAX_LENGTH);
        buf.incr_len(sdnv_len);
        memcpy(buf.tail_buf(len), lsa->id_.data(), len);
        buf.incr_len(len);

        // the link params
        memcpy(buf.tail_buf(sizeof(LinkParams)),
               &lsa->params_,
               sizeof(LinkParams));
        buf.incr_len(sizeof(LinkParams));
    }

    bundle->payload_.set_length(buf.len());
    bundle->payload_.write_data(buf.buf(), 0, buf.len());
}

//----------------------------------------------------------------------
bool
ShortestPathRouter::parse_lsa_bundle(Bundle* bundle, LSAVector* lsa_vec)
{
    oasys::ScratchBuffer<u_char*, 256> buf;
    bundle->payload_.read_data(0, bundle->payload_.length(),
                               buf.buf(bundle->payload_.length()));

    u_char* bp = buf.buf();
    size_t buflen = bundle->payload_.length();
    size_t origlen = buflen;
    int sdnv_len;
    u_int64_t done = 0;
    u_int64_t count;
    u_int64_t len;

    log_debug("parse_lsa_bundle: parsing %zu byte buffer", buflen);

    if ((sdnv_len = SDNV::decode(bp, buflen, &count)) < 0) {
tooshort:
        log_warn("parse_lsa_bundle: message too short at byte %zu/%zu "
                 "(%llu/%llu processed)",
                 origlen - buflen, origlen, U64FMT(done), U64FMT(count));
        return false;
    }
    bp     += sdnv_len;
    buflen -= sdnv_len;
    
    if (count == 0) {
        log_warn("parse_lsa_bundle: got vector of %llu LSA elements", U64FMT(count));
        return false;
    }

    log_debug("parse_lsa_bundle: got vector of %llu LSA elements", U64FMT(count));
    for (done = 0; done < count; ++done) {
        lsa_vec->push_back(LSA());
        LSA* lsa = &(lsa_vec->back());

        //
        // the source eid
        //
        if ((sdnv_len = SDNV::decode(bp, buflen, &len)) < 0)
            goto tooshort;
        bp     += sdnv_len;
        buflen -= sdnv_len;
        
        if (len > buflen)
            goto tooshort;
        
        if (! lsa->source_.assign((char*)bp, len)) {
invalid_eid:
            log_warn("parse_lsa_bundle: got invalid eid '%.*s' at byte %zu/%zu "
                     "(%llu/%llu processed)",
                     (int)len, (char*)bp, origlen - buflen, origlen, U64FMT(done), U64FMT(count));
            return false;
        }
        bp     += len;
        buflen -= len;

        //
        // the dest eid
        //
        if ((sdnv_len = SDNV::decode(bp, buflen, &len)) < 0)
            goto tooshort;
        bp     += sdnv_len;
        buflen -= sdnv_len;
        
        if (len > buflen)
            goto tooshort;
        
        if (! lsa->dest_.assign((char*)bp, len))
            goto invalid_eid;

        bp     += len;
        buflen -= len;

        //
        // the link id
        //
        if ((sdnv_len = SDNV::decode(bp, buflen, &len)) < 0)
            goto tooshort;
        bp     += sdnv_len;
        buflen -= sdnv_len;
        
        if (len > buflen)
            goto tooshort;
        
        lsa->id_.assign((char*)bp, len);
        bp     += len;
        buflen -= len;

        //
        // the link params
        //
        if (buflen < sizeof(LinkParams))
            goto tooshort;

        memcpy(&lsa->params_, bp, sizeof(LinkParams));
        bp     += sizeof(LinkParams);
        buflen -= sizeof(LinkParams);
    }
    
    return true;
}

//----------------------------------------------------------------------
void
ShortestPathRouter::generate_lsa(LSA* lsa, RoutingGraph::Edge* edge)
{
    lsa->source_.assign(edge->source_->id_);
    lsa->dest_.assign(edge->dest_->id_);
    lsa->id_.assign(edge->info_.id_);
    
    lsa->params_.cost_ = htonl(edge->info_.params_.cost_);
    lsa->params_.delay_ = htonl(edge->info_.params_.delay_);
    lsa->params_.bw_ = htonl(edge->info_.params_.bw_);
}

//----------------------------------------------------------------------
void
ShortestPathRouter::broadcast_announcement(RoutingGraph::Edge* edge)
{
    LSAVector lsavec;

    log_debug("broadcast_announcement: formatting LSA for edge %s",
              oasys::InlineFormatter<EdgeInfo>().format(edge->info_));

    LSA lsa;
    generate_lsa(&lsa, edge);
    lsavec.push_back(lsa);

    Bundle* bundle = new TempBundle();
    bundle->dest_ = announce_eid_;
    bundle->source_ = BundleDaemon::instance()->local_eid();
    bundle->replyto_ = EndpointID::NULL_EID();
    bundle->custodian_ = EndpointID::NULL_EID();
    bundle->expiration_ = 30;
    bundle->singleton_dest_ = false;
        
    format_lsa_bundle(bundle, &lsavec);

    log_debug("broadcast_announcement: formatted LSA bundle *%p", bundle);

    multicast_bundle(bundle);
}

//----------------------------------------------------------------------
void
ShortestPathRouter::dump_state(const LinkRef& link)
{
    LSAVector lsavec;

    RoutingGraph::NodeVector::const_iterator ni;
    RoutingGraph::EdgeVector::const_iterator ei;

    for (ni = graph_.nodes().begin();
         ni != graph_.nodes().end();
         ++ni)
    {
        for (ei = (*ni)->out_edges_.begin();
             ei != (*ni)->out_edges_.end();
             ++ei)
        {
            LSA lsa;
            generate_lsa(&lsa, *ei);
            lsavec.push_back(lsa);
        }
    }

    log_debug("dump_state: generated %zu LSAs for link *%p",
              lsavec.size(), link.object());

    if (lsavec.size() == 0) {
        return;
    }
                   
    Bundle* bundle = new TempBundle();
    bundle->dest_ = announce_eid_;
    bundle->source_ = BundleDaemon::instance()->local_eid();
    bundle->replyto_ = EndpointID::NULL_EID();
    bundle->custodian_ = EndpointID::NULL_EID();
    bundle->expiration_ = 30;
    bundle->singleton_dest_ = false;
    format_lsa_bundle(bundle, &lsavec);
    
    if (link->isavailable() && (!link->isopen()) && (!link->isopening())) {
        log_debug("opening *%p because a message is intended for it",
                  link.object());
        actions_->open_link(link);
    }
        
    log_debug("dump_state: sending bundle *%p to link *%p",
              bundle, link.object());
    actions_->send_bundle(bundle, link, ForwardingInfo::COPY_ACTION,
                          CustodyTimerSpec());
}

} // namespace dtn


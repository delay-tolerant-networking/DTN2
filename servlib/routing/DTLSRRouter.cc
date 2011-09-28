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
#include <dtn-config.h>
#endif

#include <algorithm>
#include <oasys/debug/DebugUtils.h>

#include "DTLSRRouter.h"
#include "bundling/BundleDaemon.h"
#include "bundling/SDNV.h"
#include "bundling/TempBundle.h"
#include "contacts/ContactManager.h"
#include "routing/RouteTable.h"
#include "session/Session.h"

namespace oasys {

//----------------------------------------------------------------------
template<>
const char*
InlineFormatter<dtn::DTLSRRouter::EdgeInfo>
  ::format(const dtn::DTLSRRouter::EdgeInfo& ei)
{
    const char* state_str;
    if (ei.params_.state_ == dtn::DTLSRRouter::LinkParams::LINK_UP) {
         state_str = "UP";
    } else if (ei.params_.state_ == dtn::DTLSRRouter::LinkParams::LINK_DOWN) {
        state_str = "DOWN";
    } else {
        state_str = "__UNKNOWN__";
    }

    buf_.appendf("%s: state=%s cost=%u delay=%u bw=%u",
                 ei.id_.c_str(), state_str,
                 ei.params_.cost_, ei.params_.delay_, ei.params_.bw_);
    return buf_.c_str();
}

} // namespace oasys

namespace dtn {

//----------------------------------------------------------------------
class DTLSRRouter::CostWeightFn : public RoutingGraph::WeightFn {
public:
    u_int32_t operator()(const RoutingGraph::SearchInfo& info,
                         const RoutingGraph::Edge* edge)
    {
        (void)info;
        if (edge->info().params_.state_ == LinkParams::LINK_DOWN) {
            return 0xffffffff;
        }
        return edge->info().params_.cost_;
    }
};

//----------------------------------------------------------------------
class DTLSRRouter::DelayWeightFn : public RoutingGraph::WeightFn {
public:
    u_int32_t operator()(const RoutingGraph::SearchInfo& info,
                         const RoutingGraph::Edge* edge)
    {
        (void)info;

        u_int32_t downtime = (info.now_ - edge->info().last_update_).sec_;
        if (DTLSRConfig::instance()->lsa_interval_ != 0 &&
            downtime > (2 * DTLSRConfig::instance()->lsa_interval_))
        {
            // don't know anything implies the link is "down"
            return 0xffffffff;
        }
            
        if (edge->info().params_.state_ == LinkParams::LINK_DOWN) {
            return 0xffffffff;
        }
        return edge->info().params_.delay_;
    }
};

//----------------------------------------------------------------------
class DTLSRRouter::EstimatedDelayWeightFn : public RoutingGraph::WeightFn {
public:
    EstimatedDelayWeightFn(DTLSRRouter* rtr)
        : router_(rtr) {}
    
    u_int32_t operator()(const RoutingGraph::SearchInfo& info,
                         const RoutingGraph::Edge* edge)
    {
        if (edge->info().params_.state_ == LinkParams::LINK_DOWN) {

            // adjust the delay based on the uptime / downtime
            u_int32_t downtime = (info.now_ - edge->info().last_update_).sec_;
            
            // XXX/demmer for now make the bogus silly assumption that
            // it will take just as long to come back up as it's been
            // down for, plus a constant factor of 5 seconds to
            // establish the link, capped at 1 day
            //
            // really, we need a good way of succinctly characterizing
            // delay in a way that can be decayed but also take into
            // account historical data, i.e. a number that
            // grows/shrinks as the link gets worse but where a router
            // can take into account that some outages are expected
            u_int32_t ret = (downtime + 5) >> DTLSRConfig::instance()->weight_shift_;

            if (ret < 24*60*60) {
                log_debug_p("/dtn/route/graph",
                            "weight of edge %s=%u (downtime %u + 5)",
                            edge->info().id_.c_str(), ret, downtime);
            } else {
                ret = 24*60*60;
                log_warn_p("/dtn/route/graph",
                           "weight of edge %s=%u: (downtime %u exceeded max)",
                           edge->info().id_.c_str(), ret, downtime);
            }

            return ret;
        }

        u_int32_t qlen, qsize;
        // XXX/demmer use the link queue length if it's a local link
        qlen  = edge->info().params_.qcount_;
        qsize = edge->info().params_.qsize_;
        
        // based on the queue length and link estimates, calculate the
        // forwarding delay
        u_int32_t qdelay = (qlen + 1) * edge->info().params_.delay_ / 1000;
        u_int32_t bwdelay = 0;
        if (edge->info().params_.bw_ != 0) {
            // XXX/demmer is bw bits or bytes?
            bwdelay = qsize / edge->info().params_.bw_;
        }
        
        u_int32_t delay = qdelay + bwdelay;
        log_debug_p("/dtn/route/graph", "weight of edge %s=%u "
                    "(qlen %u delay %u qdelay %u qsize %u bw %u bwdelay %u)",
                    edge->info().id_.c_str(), delay,
                    qlen, edge->info().params_.delay_, qdelay,
                    qsize, edge->info().params_.bw_, bwdelay);
        
        // XXX/demmer we might want to have an estimator for bundles
        // that we've sent out en route to the destination along with
        // where and when those bundles will be at different routers.
        // then when LSAs come in, we adjust our estimates accordingly

        return delay;
    }

    DTLSRRouter* router_;
};

//----------------------------------------------------------------------
DTLSRRouter::DTLSRRouter()
    : TableBasedRouter("DTLSRRouter", "dtlsr"),
      announce_tag_("dtlsr"),
      announce_eid_("dtn://*/dtlsr?*"),
      current_lsas_("DTLSRRouter::current_lsas"),
      periodic_lsa_timer_(this),
      delayed_lsa_timer_(this)
{
    // override add_nexthop_routes since it just confuses things
    config_.add_nexthop_routes_ = false;
}

//----------------------------------------------------------------------
void
DTLSRRouter::initialize()
{
    TableBasedRouter::initialize();
    
    const EndpointID& local_eid = BundleDaemon::instance()->local_eid();

    if (local_eid.scheme_str() != "dtn") {
        log_crit("cannot use DTLSR with a local eid not in the 'dtn' scheme");
        exit(1);
    }
    
    local_node_ = graph_.add_node(local_eid.str(), NodeInfo());

    EndpointIDPattern admin_eid = local_eid;
    admin_eid.append_service_tag(announce_tag_);
    reg_ = new Reg(this, admin_eid);
    
    log_info("initializing: local_eid %s weight_fn %s",
             local_eid.c_str(),
             DTLSRConfig::weight_fn_to_str(config()->weight_fn_));
             
    switch (config()->weight_fn_) {
    case DTLSRConfig::COST:
        weight_fn_ = new CostWeightFn();
        break;
        
    case DTLSRConfig::DELAY:
        weight_fn_ = new DelayWeightFn();
        break;

    case DTLSRConfig::ESTIMATED_DELAY:
        weight_fn_ = new EstimatedDelayWeightFn(this);
        break;
    }
    
    if (config()->lsa_interval_ != 0) {
        periodic_lsa_timer_.set_interval(config()->lsa_interval_ * 1000);
        periodic_lsa_timer_.schedule_in(config()->lsa_interval_ * 1000);
    }
}

//----------------------------------------------------------------------
void
DTLSRRouter::get_routing_state(oasys::StringBuffer* buf)
{
//     invalidate_routes();
//     recompute_routes();
    
    buf->appendf("DTLSR Routing Graph:\n*%p", &graph_);
    buf->appendf("Current routing table:\n");
    TableBasedRouter::get_routing_state(buf);
}

//----------------------------------------------------------------------
bool
DTLSRRouter::can_delete_bundle(const BundleRef& bundle)
{
    if (! TableBasedRouter::can_delete_bundle(bundle)) {
        return false;
    }

    if (current_lsas_.contains(bundle)) {
        log_debug("can_delete_bundle(%u): current lsa", bundle->bundleid());
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
void
DTLSRRouter::delete_bundle(const BundleRef& bundle)
{
    TableBasedRouter::delete_bundle(bundle);

    if (current_lsas_.contains(bundle))
    {
        log_crit("deleting bundle id %u whilea still on current lsas list",
                 bundle->bundleid());
        current_lsas_.erase(bundle);
    }
}

//----------------------------------------------------------------------
void
DTLSRRouter::handle_link_created(LinkCreatedEvent* e)
{
    // XXX/demmer TODO: add the edge and initialize downtime_pct based
    // on the link type.

    invalidate_routes();
    recompute_routes();
    TableBasedRouter::handle_link_created(e);
}

//----------------------------------------------------------------------
void
DTLSRRouter::handle_bundle_received(BundleReceivedEvent* e)
{
//     if (time_to_age_routes()) {
//         recompute_routes();
//     }

    TableBasedRouter::handle_bundle_received(e);
}

//----------------------------------------------------------------------
void
DTLSRRouter::handle_bundle_expired(BundleExpiredEvent* e)
{
    Bundle* bundle = e->bundleref_.object();

    // check if this is one of the current LSAs if so, we
    // need to drop our retention constraint and let it expire
    //
    // XXX/demmer perhaps we should do something more drastic like
    // remove the node from the routing graph?
    oasys::ScopeLock l(current_lsas_.lock(),
                       "DTLSRRouter::handle_bundle_expired");

    if (current_lsas_.contains(bundle))
    {
        log_notice("current lsa for %s expired... kicking links",
                   bundle->source().c_str());
        handle_lsa_expired(bundle);
        current_lsas_.erase(bundle);
    }
    
    TableBasedRouter::handle_bundle_expired(e);
}
    
//----------------------------------------------------------------------
void
DTLSRRouter::handle_contact_up(ContactUpEvent* e)
{
    const LinkRef& link = e->contact_->link();
    if (link->remote_eid() == EndpointID::NULL_EID()) {
        log_warn("can't handle link %s: no remote endpoint id",
                 link->name());
        return;
    }

    EdgeInfo ei(link->name());
    // XXX/demmer what about bw/delay/capacity?
    ei.params_.cost_ = link->params().cost_;
    
    RoutingGraph::Node* remote_node =
        graph_.find_node(link->remote_eid().str());

    if (remote_node == NULL) {
        remote_node = graph_.add_node(link->remote_eid().str(), NodeInfo());
    }

    RoutingGraph::Edge* edge = graph_.find_edge(local_node_, remote_node, ei);
    if (edge == NULL) {
        edge = graph_.add_edge(local_node_, remote_node, ei);
    } else {
        edge->mutable_info().params_.state_ = LinkParams::LINK_UP;
    }

    // always calculate the new routing graph now
    invalidate_routes();
    recompute_routes();
    
    // schedule a new lsa to include the new link
    schedule_lsa();

    // now let the superclass have a turn at the event
    TableBasedRouter::handle_contact_up(e);
}

//----------------------------------------------------------------------
void
DTLSRRouter::handle_contact_down(ContactDownEvent* e)
{
    const LinkRef& link = e->contact_->link();

    RoutingGraph::Node* remote_node =
        graph_.find_node(link->remote_eid().str());

    if (remote_node == NULL) {
        log_err("contact down event for link %s: node not in routing graph",
                link->name());
        return;
    }

    RoutingGraph::Edge* edge = NULL;
    RoutingGraph::EdgeVector::const_iterator iter;
    for (iter = local_node_->out_edges().begin();
         iter != local_node_->out_edges().end(); ++iter)
    {
        if ((*iter)->info().id_ == link->name_str()) {
            edge = *iter;
            break;
        }
    }
    
    if (edge == NULL) {
        log_err("handle_contact_down: can't find link *%p", link.object());
        return;
    }
        
    edge->mutable_info().params_.state_ = LinkParams::LINK_DOWN;

    // calculate the new routing graph
    invalidate_routes();
    recompute_routes();

    // inform peers
    schedule_lsa();
    
    if (! config()->keep_down_links_) {
        remove_edge(edge);
    }
    
    TableBasedRouter::handle_contact_down(e);
}

//----------------------------------------------------------------------
void
DTLSRRouter::handle_link_deleted(LinkDeletedEvent* e)
{
    TableBasedRouter::handle_link_deleted(e);
    NOTREACHED;
    
    // XXX/demmer fixme
    
    RoutingGraph::EdgeVector::const_iterator iter;
    for (iter = local_node_->out_edges().begin();
         iter != local_node_->out_edges().end(); ++iter)
    {
        if ((*iter)->info().id_ == e->link_->name()) {
            remove_edge(*iter);
            return;
        }
    }

    log_err("handle_link_deleted: can't find link *%p", e->link_.object());
}

//----------------------------------------------------------------------
void
DTLSRRouter::handle_registration_added(RegistrationAddedEvent* event)
{
    Registration* reg = event->registration_;
    const EndpointID& local_eid = BundleDaemon::instance()->local_eid();
    
    TableBasedRouter::handle_registration_added(event);

    // to handle registration for endpoint identifiers that aren't
    // covered by our local_eid pattern, we add a node to the graph
    // with an infinite-bandwidth edge from our local node to it.
    //
    // session registrations don't get announced, except for custody
    // ones which get the 'dtn-session:' prefix added on
    if (reg->endpoint().subsume(local_eid)) {
        return; // nothing to do
    }

    std::string eid;
    if (reg->session_flags() == 0)
    {
        eid = reg->endpoint().str();
    }
    else if (reg->session_flags() & Session::CUSTODY)
    {
        eid = std::string("dtn-session:") + reg->endpoint().str();
    }
    else
    {
        return; // ignore non-custody registrations
    }
    
    RoutingGraph::Node* node = graph_.find_node(eid);
    if (node == NULL) {
        log_debug("handle_registration added: adding new graph node for %s",
                  eid.c_str());
        node = graph_.add_node(eid, NodeInfo());
    }

    EdgeInfo ei(std::string("reg-") + eid);
    ei.is_registration_ = true;
    ei.params_.bw_      = 0xffffffff;
    
    RoutingGraph::Edge* edge = graph_.find_edge(local_node_, node, ei);
    if (edge == NULL) {
        log_debug("handle_registration added: adding new edge node for %s",
                  ei.id_.c_str());
        edge = graph_.add_edge(local_node_, node, ei);
        
        // send a new lsa to announce the new edge
        schedule_lsa();
    }
}

//----------------------------------------------------------------------
void
DTLSRRouter::remove_edge(RoutingGraph::Edge* edge)
{
    log_debug("remove_edge %s", edge->info().id_.c_str());
    bool ok = graph_.del_edge(local_node_, edge);
    ASSERT(ok);
}

//----------------------------------------------------------------------
bool
DTLSRRouter::time_to_age_routes()
{
    u_int32_t elapsed = last_update_.elapsed_ms() / 1000;
    if (elapsed > config()->aging_interval_) {
        return true;
    }
    return false;
}

//----------------------------------------------------------------------
void
DTLSRRouter::invalidate_routes()
{
    log_debug("invalidating routes");
    last_update_.sec_ = 0;
}

//----------------------------------------------------------------------
bool
DTLSRRouter::is_dynamic_route(RouteEntry* entry)
{
    if (entry->info() == NULL) {
        return false;
    }
    
    RouteInfo* info = dynamic_cast<RouteInfo*>(entry->info());
    if (info != NULL) {
        return true;
    }
    
    return false;
}

//----------------------------------------------------------------------
void
DTLSRRouter::recompute_routes()
{
//     // XXX/demmer make this a parameter
//     u_int32_t elapsed = last_update_.elapsed_ms() / 1000;
//     if (elapsed < config()->recompute_delay_) {
//         log_debug("not recomputing routes since %u ms elapsed since last update",
//                   elapsed);
//         return;
//     }

    log_debug("recomputing all routes");
    last_update_.get_time();

    route_table_->del_matching_entries(is_dynamic_route);

    // loop through all the nodes in the graph, finding the right
    // route and re-adding it
    RoutingGraph::NodeVector::const_iterator iter;
    for (iter = graph_.nodes().begin(); iter != graph_.nodes().end(); ++iter) {
        const RoutingGraph::Node* dest = *iter;

        if (dest == local_node_) {
            continue;
        }

        EndpointIDPattern dest_eid(dest->id());
        dest_eid.append_service_tag("*");
        ASSERT(dest_eid.valid());

        // XXX/demmer this should include more criteria for
        // classification, i.e. the priority class, perhaps the size
        // limit, etc
        RoutingGraph::Edge* edge = graph_.best_next_hop(local_node_, dest,
                                                        weight_fn_);
        if (edge == NULL) {
//            log_warn("no route to destination %s", dest->id().c_str());
            continue;
        }

        ContactManager* cm = BundleDaemon::instance()->contactmgr();
        LinkRef link = cm->find_link(edge->info().id_.c_str());
        if (link == NULL) {

            // it's possible that this is a local registration link,
            // so just ignore it
            log_debug("link %s not in local link table... ignoring it",
                      edge->info().id_.c_str());
            continue;
        }

        log_debug("recompute_routes: adding route for %s to link %s",
                  dest_eid.c_str(), link->name());

        // XXX/demmer shouldn't this call check_next_hop??
        RouteEntry* e = new RouteEntry(dest_eid, link);
        e->set_info(new RouteInfo(edge));
        route_table_->add_entry(e);
    }

    // go through all bundles and re-route them
    handle_changed_routes();
}

//----------------------------------------------------------------------
DTLSRRouter::Reg::Reg(DTLSRRouter* router,
                      const EndpointIDPattern& eid)
   : Registration(DTLSR_REGID, eid, Registration::DEFER, Registration::NONE, 
                   /* XXX/demmer session_flags */ 0, 0),
                   
      router_(router)
{
    logpathf("%s/reg", router->logpath()),
    BundleDaemon::post(new RegistrationAddedEvent(this, EVENTSRC_ADMIN));
}

//----------------------------------------------------------------------
void
DTLSRRouter::Reg::deliver_bundle(Bundle* bundle)
{
    // XXX/demmer yuck
    if (bundle->source() == BundleDaemon::instance()->local_eid()) {
        log_info("ignoring bundle sent by self");
        return;
    }

    u_char type;
    bundle->payload().read_data(0, 1, &type);

    if (type == DTLSR::MSG_LSA) {
        log_info("got LSA bundle *%p",bundle);
        
        LSA lsa;
        if (!DTLSR::parse_lsa_bundle(bundle, &lsa)) {
            log_warn("error parsing LSA");
            goto bail;
        }
        
        router_->handle_lsa(bundle, &lsa);
    } else {
        log_err("unknown message type %d", type);
    }

bail:
    BundleDaemon::post_at_head(new BundleDeliveredEvent(bundle, this));
}

//----------------------------------------------------------------------
bool
DTLSRRouter::update_current_lsa(RoutingGraph::Node* node,
                                Bundle* bundle, u_int64_t seqno)
{
    bool found_stale_lsa = false;
    if (seqno <= node->info().last_lsa_seqno_ &&
        bundle->creation_ts().seconds_ < node->info().last_lsa_creation_ts_)
    {
        log_info("update_current_lsa: "
                 "ignoring stale LSA (seqno %llu <= last %llu, "
                 "creation_ts %llu <= last %llu)",
                 seqno, node->info().last_lsa_seqno_,
                 bundle->creation_ts().seconds_,
                 node->info().last_lsa_creation_ts_);

        // suppress forwarding of the stale LSA
        bundle->fwdlog()->add_entry(EndpointIDPattern::WILDCARD_EID(),
                                    ForwardingInfo::FORWARD_ACTION,
                                    ForwardingInfo::SUPPRESSED);
        
        return false;
    }
    else
    {
        log_info("update_current_lsa: "
                 "got new LSA (seqno %llu > last %llu || "
                 "creation_ts %llu > last %llu)",
                 seqno, node->info().last_lsa_seqno_,
                 bundle->creation_ts().seconds_,
                 node->info().last_lsa_creation_ts_);
    }

    oasys::ScopeLock l(current_lsas_.lock(),
                       "DTLSRRouter::update_current_lsa");
    for (BundleList::iterator iter = current_lsas_.begin();
         iter != current_lsas_.end(); ++iter)
    {
        if ((*iter)->source() == node->id()) {
            // be careful not to let the bundle reference count drop
            // to zero by keeping a local reference until we can make
            // sure it's at least in the NotNeededEvent
            BundleRef stale_lsa("DTLSRRouter::update_current_lsa");
            stale_lsa = *iter;

            current_lsas_.erase(iter);

            log_debug("cancelling pending transmissions for *%p",
                      stale_lsa.object());

            stale_lsa->fwdlog()->add_entry(EndpointIDPattern::WILDCARD_EID(),
                                           ForwardingInfo::FORWARD_ACTION,
                                           ForwardingInfo::SUPPRESSED);
            
            BundleDaemon::post_at_head(
                new BundleDeleteRequest(stale_lsa.object(),
                                        BundleProtocol::REASON_NO_ADDTL_INFO));
            found_stale_lsa = true;
            
//             oasys::StringBuffer buf("Stale LSA: ");
//             bundle->format_verbose(&buf);
//             log_multiline(oasys::LOG_INFO, buf.c_str());
            
            if (node->info().last_lsa_seqno_ == 0) {
                NOTREACHED;
            }
            break;
        }
    }

    if (node->info().last_lsa_seqno_ == 0) {
        ASSERT(!found_stale_lsa);
        log_info("update_current_lsa: "
                 "first LSA from %s (seqno %llu)",
                 node->id().c_str(), seqno);
    } else {
        log_info("update_current_lsa: "
                 "replaced %s LSA from %s (seqno %llu) with latest (%llu)",
                 found_stale_lsa ? "stale" : "expired",
                 node->id().c_str(), node->info().last_lsa_seqno_, seqno);
    }

    node->mutable_info().last_lsa_seqno_ = seqno;
    node->mutable_info().last_lsa_creation_ts_ = bundle->creation_ts().seconds_;
    current_lsas_.push_back(bundle);
    return true;
}

//----------------------------------------------------------------------
void
DTLSRRouter::handle_lsa(Bundle* bundle, LSA* lsa)
{
    // First check the LSA bundle destination to see if the sender is
    // in a different area
    std::string lsa_area = bundle->dest().uri().query_value("area");
    if (config()->area_ != lsa_area) {
        log_debug("handle_lsa: ignoring LSA since area %s != local area %s",
                  lsa_area.c_str(), config()->area_.c_str());

        bundle->fwdlog()->add_entry(EndpointIDPattern::WILDCARD_EID(),
                                    ForwardingInfo::FORWARD_ACTION,
                                    ForwardingInfo::SUPPRESSED);
        return;
    }
    
    const EndpointID& source = bundle->source();
    
    RoutingGraph::Node* a = graph_.find_node(source.str());
    if (a == NULL) {
        log_debug("handle_lsa: adding new source node %s",
                  source.c_str());
        a = graph_.add_node(source.str(), NodeInfo());
    }

    if (! update_current_lsa(a, bundle, lsa->seqno_)) {
        return; // stale lsa
    }

    // don't send the LSA back where we got it from
    ForwardingInfo info;
    if (bundle->fwdlog()->get_latest_entry(ForwardingInfo::RECEIVED, &info))
    {
        log_debug("handle_lsa: suppressing transmission back to %s",
                  info.remote_eid().c_str());
        bundle->fwdlog()->add_entry(info.remote_eid(),
                                    ForwardingInfo::FORWARD_ACTION,
                                    ForwardingInfo::SUPPRESSED);
    }
    
    // XXX/demmer here we should drop all the links that aren't
    // present in the LSA...

    // Handle all the link announcements
    for (LinkStateVec::iterator iter = lsa->links_.begin();
         iter != lsa->links_.end(); ++iter)
    {
        LinkState* ls = &(*iter);

        // check for the case where we're re-receiving an LSA that was
        // sent previously, but our link configuration has changed so
        // we no longer have the link
        if (a == local_node_) {
            LinkRef link("DTLSRRouter::handle_lsa");
            link = BundleDaemon::instance()->contactmgr()->
                   find_link(ls->id_.c_str());
            if (link == NULL) {
                log_warn("local link %s in LSA but no longer exists, ignoring...",
                         ls->id_.c_str());
                continue;
            }
        }

        // find or add the node
        RoutingGraph::Node* b = graph_.find_node(ls->dest_.str());
        if (!b) {
            log_debug("handle_lsa: %s adding new dest node %s",
                      source.c_str(), ls->dest_.c_str());
            b = graph_.add_node(ls->dest_.str(), NodeInfo());
        }

        // try to find and update the edge in the graph, otherwise add it
        RoutingGraph::Edge *e = NULL;
        RoutingGraph::EdgeVector::const_iterator ei;
        for (ei = a->out_edges().begin(); ei != a->out_edges().end(); ++ei) {
            if ((*ei)->info().id_ == ls->id_) {
                e = *ei;
                break;
            }
        }

        if (e == NULL) {
            e = graph_.add_edge(a, b, EdgeInfo(ls->id_, ls->params_));
            
            log_debug("handle_lsa: added new edge %s from %s -> %s",
                      oasys::InlineFormatter<EdgeInfo>().format(e->info()),
                      a->id().c_str(), b->id().c_str());
        } else {
            e->mutable_info().params_ = ls->params_;
            
            log_debug("handle_lsa: updated edge %s from %s -> %s",
                      oasys::InlineFormatter<EdgeInfo>().format(e->info()),
                      a->id().c_str(), b->id().c_str());
        }

        // XXX/demmer fold this into a parameter update method
        e->mutable_info().last_update_.get_time();
    }

    recompute_routes();
}

//----------------------------------------------------------------------
void
DTLSRRouter::handle_lsa_expired(Bundle* bundle)
{
    // XXX/demmer this is bogus -- we don't want to drop all routes
    // just b/c we lost an LSA in all cases, only when we're using the
    // long-lived LSA bundles
    (void)bundle;
    
    /*
    const EndpointID& source = bundle->source_;
    
    RoutingGraph::Node* a = graph_.find_node(source.str());
    ASSERT(a != NULL);

    RoutingGraph::EdgeVector::const_iterator ei;
    for (ei = a->out_edges().begin(); ei != a->out_edges().end(); ++ei) {
        (*ei)->mutable_info().params_.state_ = LinkParams::LINK_DOWN;
    }

    recompute_routes();
    */
}

//----------------------------------------------------------------------
void
DTLSRRouter::generate_link_state(LinkState* ls,
                                 RoutingGraph::Edge* edge,
                                 const LinkRef& link)
{
    ls->dest_.assign(edge->dest()->id());
    ls->id_.assign(edge->info().id_);
    ls->params_ = edge->info().params_;

    // XXX/demmer maybe the edge info should be tied to the link
    // itself somehow?
    if (link != NULL) {
        ls->params_.qcount_ = link->bundles_queued();
        ls->params_.qsize_  = link->bytes_queued();
    }
    
    if (edge->info().last_update_.sec_ != 0) {
        ls->elapsed_ = edge->info().last_update_.elapsed_ms();
    } else {
        ls->elapsed_ = 0;
    }
    edge->mutable_info().last_update_.get_time();
}

//----------------------------------------------------------------------
void
DTLSRRouter::TransmitLSATimer::timeout(const struct timeval& now)
{
    (void)now;
    router_->send_lsa();
    if (interval_ != 0) {
        schedule_in(interval_);
    }
}

//----------------------------------------------------------------------
void
DTLSRRouter::schedule_lsa()
{
    if (delayed_lsa_timer_.pending()) {
        log_debug("schedule_lsa: delayed LSA transmission already pending");
        return;
    }

    u_int elapsed = (oasys::Time::now() - last_lsa_transmit_).sec_;
    if (elapsed > config()->min_lsa_interval_)
    {
        log_debug("schedule_lsa: %u seconds since last LSA transmission, "
                  "sending one immediately", elapsed);
        send_lsa();
    }
    else
    {
        u_int delay = std::min(config()->min_lsa_interval_,
                               config()->min_lsa_interval_ - elapsed);
        log_debug("schedule_lsa: %u seconds since last LSA transmission, "
                  "min interval %u, delaying LSA for %u seconds",
                  elapsed, config()->min_lsa_interval_, delay);
        delayed_lsa_timer_.schedule_in(delay * 1000);
    }
}

//----------------------------------------------------------------------
void
DTLSRRouter::send_lsa()
{
    char tmp[64]; (void)tmp;
    LSA lsa;

    // the contents of last_lsa_seqno get updated in the call to
    // update_current_lsa below
    lsa.seqno_ = local_node_->info().last_lsa_seqno_ + 1;
    
    RoutingGraph::EdgeVector::iterator ei;
    for (ei = local_node_->out_edges().begin();
         ei != local_node_->out_edges().end();
         ++ei)
    {
        LinkState ls;
        LinkRef link("DTLSRRouter::send_lsa");

        // if the edge is a local registration, there's no link, we
        // just pretend there's a zero-delay, infinite-bandwidth link
        // to the other endpoint
        if (! (*ei)->info().is_registration_) {
            link = BundleDaemon::instance()->contactmgr()->
                   find_link((*ei)->info().id_.c_str());
            ASSERT(link != NULL);
        }
        
        generate_link_state(&ls, *ei, link);
        lsa.links_.push_back(ls);
    }

    log_debug("send_lsa: generated %zu link states for local node",
              lsa.links_.size());

    Bundle* bundle = new TempBundle();

    if (config()->area_ != "") {
        snprintf(tmp, sizeof(tmp), "dtn://*/%s?area=%s;lsa_seqno=%llu",
                 announce_tag_, config()->area_.c_str(), lsa.seqno_);
    } else {
        snprintf(tmp, sizeof(tmp), "dtn://*/%s?lsa_seqno=%llu",
                 announce_tag_, lsa.seqno_);
    }
    bundle->mutable_dest()->assign(tmp);
    
    bundle->mutable_source()->assign(BundleDaemon::instance()->local_eid());
    bundle->mutable_replyto()->assign(EndpointID::NULL_EID());
    bundle->mutable_custodian()->assign(EndpointID::NULL_EID());
    bundle->set_expiration(config()->lsa_lifetime_);
    bundle->set_singleton_dest(false);
    DTLSR::format_lsa_bundle(bundle, &lsa);

    update_current_lsa(local_node_, bundle, lsa.seqno_);
    
    log_debug("send_lsa: formatted LSA bundle *%p", bundle);

    // XXX/demmer this would really be better done with
    // BundleActions::inject_bundle
    BundleDaemon::post_at_head(new BundleReceivedEvent(bundle, EVENTSRC_ROUTER));

    last_lsa_transmit_.get_time();
}

} // namespace dtn


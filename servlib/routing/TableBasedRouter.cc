/*
 *    Copyright 2005-2006 Intel Corporation
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
#  include <dtn-config.h>
#endif

#include "TableBasedRouter.h"
#include "RouteTable.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleDaemon.h"
#include "contacts/Contact.h"
#include "contacts/ContactManager.h"
#include "contacts/Link.h"

namespace dtn {

//----------------------------------------------------------------------
TableBasedRouter::TableBasedRouter(const char* classname,
                                   const std::string& name)
    : BundleRouter(classname, name),
      dupcache_(1024)
{
    route_table_ = new RouteTable(name);
}

//----------------------------------------------------------------------
TableBasedRouter::~TableBasedRouter()
{
    delete route_table_;
}

//----------------------------------------------------------------------
void
TableBasedRouter::add_route(RouteEntry *entry)
{
    route_table_->add_entry(entry);
    dupcache_.evict_all();
    reroute_all_bundles();        
}

//----------------------------------------------------------------------
void
TableBasedRouter::del_route(const EndpointIDPattern& dest)
{
    route_table_->del_entries(dest);
    dupcache_.evict_all();
    // XXX/demmer this needs to reroute bundles, cancelling them from
    // their old route destinations
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_event(BundleEvent* event)
{
    dispatch_event(event);
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_bundle_received(BundleReceivedEvent* event)
{
    Bundle* bundle = event->bundleref_.object();
    log_debug("handle bundle received: *%p", bundle);

    if (dupcache_.is_duplicate(bundle)) {
        log_info("ignoring duplicate bundle: *%p", bundle);
        BundleDaemon::post_at_head(new BundleNotNeededEvent(bundle));
        return;
    }
    
    route_bundle(bundle);
}

//----------------------------------------------------------------------
void
TableBasedRouter::remove_from_deferred(const BundleRef& bundle, int actions)
{
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    oasys::ScopeLock l(cm->lock(), "TableBasedRouter::remove_from_deferred");

    const LinkSet* links = cm->links();
    LinkSet::const_iterator iter;
    for (iter = links->begin(); iter != links->end(); ++iter) {
        DeferredList* deferred = deferred_list(*iter);
        ForwardingInfo info;
        if (deferred->find(bundle, &info))
        {
            if (info.action() & actions) {
                log_debug("removing bundle *%p from link *%p deferred list",
                          bundle.object(), (*iter).object());
                deferred->del(bundle);
            }
        }
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    const BundleRef& bundle = event->bundleref_;
    log_debug("handle bundle transmitted: *%p", bundle.object());

    // if the bundle has a deferred single-copy transmission for
    // forwarding on any links, then remove the forwarding log entries
    remove_from_deferred(bundle, ForwardingInfo::FORWARD_ACTION);
    
    // check if the transmission means that we can send another bundle
    // on the link
    const LinkRef& link = event->contact_->link();
    check_next_hop(link);
}

//----------------------------------------------------------------------
void
TableBasedRouter::delete_bundle(const BundleRef& bundle)
{
    log_debug("delete bundle: *%p", bundle.object());
    remove_from_deferred(bundle, ForwardingInfo::ANY_ACTION);
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_bundle_cancelled(BundleSendCancelledEvent* event)
{
    Bundle* bundle = event->bundleref_.object();
    log_debug("handle bundle cancelled: *%p", bundle);

    // if the bundle has expired, we don't want to reroute it.
    // XXX/demmer this might warrant a more general handling instead?
    if (!bundle->expired()) {
        route_bundle(bundle);
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_route_add(RouteAddEvent* event)
{
    add_route(event->entry_);
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_route_del(RouteDelEvent* event)
{
    del_route(event->dest_);
}

//----------------------------------------------------------------------
void
TableBasedRouter::add_nexthop_route(const LinkRef& link)
{
    // If we're configured to do so, create a route entry for the eid
    // specified by the link when it connected, using the
    // scheme-specific code to transform the URI to wildcard
    // the service part
    EndpointID eid = link->remote_eid();
    if (config_.add_nexthop_routes_ && eid != EndpointID::NULL_EID())
    { 
        EndpointIDPattern eid_pattern(link->remote_eid());

        // attempt to build a route pattern from link's remote_eid
        if (!eid_pattern.append_service_wildcard())
            // else assign remote_eid as-is
            eid_pattern.assign(link->remote_eid());

        // XXX/demmer this shouldn't call get_matching but instead
        // there should be a RouteTable::lookup or contains() method
        // to find the entry
        RouteEntryVec ignored;
        if (route_table_->get_matching(eid_pattern, link, &ignored) == 0) {
            RouteEntry *entry = new RouteEntry(eid_pattern, link);
            entry->set_action(ForwardingInfo::FORWARD_ACTION);
            add_route(entry);
        }
    }
}

//----------------------------------------------------------------------
bool
TableBasedRouter::should_fwd(const Bundle* bundle, RouteEntry* route)
{
    if (route == NULL)
        return false;

    return BundleRouter::should_fwd(bundle, route->link(), route->action());
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_contact_up(ContactUpEvent* event)
{
    LinkRef link = event->contact_->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    add_nexthop_route(link);
    check_next_hop(link);

    // check if there's a pending reroute timer on the link, and if
    // so, cancel it.
    // 
    // note that there's a possibility that a link just bounces
    // between up and down states but can't ever really send a bundle
    // (or part of one), which we don't handle here since we can't
    // distinguish that case from one in which the CL is actually
    // sending data, just taking a long time to do so.

    RerouteTimerMap::iterator iter = reroute_timers_.find(link->name_str());
    if (iter != reroute_timers_.end()) {
        log_debug("link %s reopened, cancelling reroute timer", link->name());
        RerouteTimer* t = iter->second;
        reroute_timers_.erase(iter);
        t->cancel();
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_contact_down(ContactDownEvent* event)
{
    LinkRef link = event->contact_->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    // if there are any bundles queued on the link when it goes down,
    // schedule a timer to cancel those transmissions and reroute the
    // bundles in case the link takes too long to come back up

    size_t num_queued = link->queue()->size();
    if (num_queued != 0) {
        RerouteTimerMap::iterator iter = reroute_timers_.find(link->name_str());
        if (iter == reroute_timers_.end()) {
            log_debug("link %s went down with %zu bundles queued, "
                      "scheduling reroute timer in %u seconds",
                      link->name(), num_queued,
                      link->params().potential_downtime_);
            RerouteTimer* t = new RerouteTimer(this, link);
            t->schedule_in(link->params().potential_downtime_ * 1000);
            
            reroute_timers_[link->name_str()] = t;
        }
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::RerouteTimer::timeout(const struct timeval& now)
{
    (void)now;
    router_->reroute_bundles(link_);
}

//----------------------------------------------------------------------
void
TableBasedRouter::reroute_bundles(const LinkRef& link)
{
    ASSERT(!link->isdeleted());

    // if the reroute timer fires, the link should be down and there
    // should be at least one bundle queued on it.
    if (link->state() != Link::UNAVAILABLE) {
        log_warn("reroute timer fired but link *%p state is %s, not UNAVAILABLE",
                 link.object(), Link::state_to_str(link->state()));
        return;
    }
    
    log_debug("reroute timer fired -- cancelling %zu bundles on link *%p",
              link->queue()->size(), link.object());
    
    // cancel the queued transmissions and rely on the
    // BundleSendCancelledEvent handler to actually reroute the
    // bundles, being careful when iterating through the lists to
    // avoid STL memory clobbering since cancel_bundle removes from
    // the list
    oasys::ScopeLock l(link->queue()->lock(),
                       "TableBasedRouter::reroute_bundles");
    BundleRef bundle("TableBasedRouter::reroute_bundles");
    while (! link->queue()->empty()) {
        bundle = link->queue()->front();
        actions_->cancel_bundle(bundle.object(), link);
        ASSERT(! bundle->is_queued_on(link->queue()));
    }

    // there should never have been any in flight since the link is
    // unavailable
    ASSERT(link->inflight()->empty());
}    

//----------------------------------------------------------------------
void
TableBasedRouter::handle_link_available(LinkAvailableEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    // if it is a discovered link, we typically open it
    if (config_.open_discovered_links_ &&
        !link->isopen() &&
        link->type() == Link::OPPORTUNISTIC &&
        event->reason_ == ContactEvent::DISCOVERY)
    {
        actions_->open_link(link);
    }
    
    // check if there's anything to be forwarded to the link
    check_next_hop(link);
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_link_created(LinkCreatedEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    link->set_router_info(new DeferredList(logpath(), link));
                          
    add_nexthop_route(link);
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_link_deleted(LinkDeletedEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);

    route_table_->del_entries_for_nexthop(link);

    RerouteTimerMap::iterator iter = reroute_timers_.find(link->name_str());
    if (iter != reroute_timers_.end()) {
        log_debug("link %s deleted, cancelling reroute timer", link->name());
        RerouteTimer* t = iter->second;
        reroute_timers_.erase(iter);
        t->cancel();
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_custody_timeout(CustodyTimeoutEvent* event)
{
    // the bundle daemon should have recorded a new entry in the
    // forwarding log for the given link to note that custody transfer
    // timed out, and of course the bundle should still be in the
    // pending list.
    //
    // therefore, trying again to forward the bundle should match
    // either the previous link or any other route
    route_bundle(event->bundle_.object());
}

//----------------------------------------------------------------------
void
TableBasedRouter::get_routing_state(oasys::StringBuffer* buf)
{
    buf->appendf("Route table for %s router:\n\n", name_.c_str());
    route_table_->dump(buf);
}

//----------------------------------------------------------------------
void
TableBasedRouter::tcl_dump_state(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(route_table_->lock(),
                       "TableBasedRouter::tcl_dump_state");

    RouteEntryVec::const_iterator iter;
    for (iter = route_table_->route_table()->begin();
         iter != route_table_->route_table()->end(); ++iter)
    {
        const RouteEntry* e = *iter;
        buf->appendf(" {%s %s source_eid %s priority %d} ",
                     e->dest_pattern().c_str(),
                     e->link()->name(),
                     e->source_pattern().c_str(),
                     e->priority());
    }
}

//----------------------------------------------------------------------
bool
TableBasedRouter::fwd_to_nexthop(Bundle* bundle, RouteEntry* route)
{
    const LinkRef& link = route->link();

    // if the link is available and not open, open it
    if (link->isavailable() && (!link->isopen()) && (!link->isopening())) {
        log_debug("opening *%p because a message is intended for it",
                  link.object());
        actions_->open_link(link);
    }

    // XXX/demmer maybe this should queue_bundle immediately instead
    // of waiting for the first contact_up event??
    
    // if the link is open and has space in the queue, then queue the
    // bundle for transmission there
    if (link->isopen() && !link->queue_is_full()) {
        log_debug("queuing *%p on *%p", bundle, link.object());
        actions_->queue_bundle(bundle, link, route->action(),
                               route->custody_spec());
        return true;
    }
    
    // otherwise we can't send the bundle now, so put it on the link's
    // deferred list and log reason why we can't forward it
    DeferredList* deferred = deferred_list(link);
    if (! bundle->is_queued_on(deferred->list())) {
        BundleRef bref(bundle, "TableBasedRouter::fwd_to_nexthop");
        ForwardingInfo info(ForwardingInfo::NONE,
                            route->action(),
                            link->name_str(),
                            0xffffffff,
                            link->remote_eid(),
                            route->custody_spec());
        deferred->add(bref, info);
    } else {
        log_warn("bundle *%p already exists on deferred list of link *%p",
                 bundle, link.object());
    }
    
    if (!link->isavailable()) {
        log_debug("can't forward *%p to *%p because link not available",
                  bundle, link.object());
    } else if (! link->isopen()) {
        log_debug("can't forward *%p to *%p because link not open",
                  bundle, link.object());
    } else if (link->queue_is_full()) {
        log_debug("can't forward *%p to *%p because link queue is full",
                  bundle, link.object());
    } else {
        log_debug("can't forward *%p to *%p", bundle, link.object());
    }

    return false;
}

//----------------------------------------------------------------------
int
TableBasedRouter::route_bundle(Bundle* bundle)
{
    RouteEntryVec matches;
    RouteEntryVec::iterator iter;

    log_debug("route_bundle: checking bundle %d", bundle->bundleid());

    // XXX/demmer fix this
    if (bundle->owner() == "DO_NOT_FORWARD") {
        log_notice("route_bundle: "
                   "ignoring bundle %d since owner is DO_NOT_FORWARD",
                   bundle->bundleid());
        return 0;
    }

    LinkRef null_link("TableBasedRouter::route_bundle");
    route_table_->get_matching(bundle->dest(), null_link, &matches);

    // sort the matching routes by priority, allowing subclasses to
    // override the way in which the sorting occurs
    sort_routes(bundle, &matches);

    log_debug("route_bundle bundle id %d: checking %zu route entry matches",
              bundle->bundleid(), matches.size());
    
    unsigned int count = 0;
    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        RouteEntry* route = *iter;
        log_debug("checking route entry %p link %s (%p)",
                  *iter, route->link()->name(), route->link().object());

        if (! should_fwd(bundle, *iter)) {
            continue;
        }

        if (deferred_list(route->link())->list()->contains(bundle)) {
            log_debug("route_bundle bundle %d: "
                      "ignoring link *%p since already deferred",
                      bundle->bundleid(), route->link().object());
            continue;
        }

        // because there may be bundles that already have deferred
        // transmission on the link, we first call check_next_hop to
        // get them into the queue before trying to route the new
        // arrival, otherwise it might leapfrog the other deferred
        // bundles
        check_next_hop(route->link());
        
        if (!fwd_to_nexthop(bundle, *iter)) {
            continue;
        }
        
        ++count;
    }

    log_debug("route_bundle bundle id %d: forwarded on %u links",
              bundle->bundleid(), count);
    return count;
}

//----------------------------------------------------------------------
void
TableBasedRouter::sort_routes(Bundle* bundle, RouteEntryVec* routes)
{
    (void)bundle;
    std::sort(routes->begin(), routes->end(), RoutePrioritySort());
}

//----------------------------------------------------------------------
void
TableBasedRouter::check_next_hop(const LinkRef& next_hop)
{
    // if the link queue doesn't have space (based on the low water
    // mark) don't do anything
    if (! next_hop->queue_has_space()) {
        log_debug("check_next_hop %s -> %s: no space in queue...",
                  next_hop->name(), next_hop->nexthop());
        return;
    }
    
    log_debug("check_next_hop %s -> %s: checking deferred bundle list...",
              next_hop->name(), next_hop->nexthop());

    // because the loop below will remove the current bundle from
    // the deferred list, invalidating any iterators pointing to its
    // position, make sure to advance the iterator before processing
    // the current bundle
    DeferredList* deferred = deferred_list(next_hop);

    oasys::ScopeLock l(deferred->list()->lock(), 
                       "TableBasedRouter::check_next_hop");
    BundleList::iterator iter = deferred->list()->begin();
    while (iter != deferred->list()->end())
    {
        if (next_hop->queue_is_full()) {
            log_debug("check_next_hop %s: link queue is full, stopping loop",
                      next_hop->name());
            break;
        }
        
        BundleRef bundle("TableBasedRouter::check_next_hop");
        bundle = *iter;
        ++iter;

        ForwardingInfo info = deferred->info(bundle);

        // if should_fwd returns false, then the bundle was either
        // already transmitted or is in flight on another node. since
        // it's possible that one of the other transmissions will
        // fail, we leave it on the deferred list for now, relying on
        // the transmitted handlers to clean up the state
        if (! BundleRouter::should_fwd(bundle.object(), next_hop,
                                       info.action()))
        {
            log_debug("check_next_hop: not forwarding to link %s",
                      next_hop->name());
            continue;
        }
        
        // if the link is available and not open, open it
        if (next_hop->isavailable() &&
            (!next_hop->isopen()) && (!next_hop->isopening()))
        {
            log_debug("check_next_hop: "
                      "opening *%p because a message is intended for it",
                      next_hop.object());
            actions_->open_link(next_hop);
        }

        // remove the bundle from the deferred list
        deferred->del(bundle);
    
        log_debug("check_next_hop: sending *%p to *%p",
                  bundle.object(), next_hop.object());
        actions_->queue_bundle(bundle.object() , next_hop,
                               info.action(), info.custody_spec());
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::reroute_all_bundles()
{
    oasys::ScopeLock l(pending_bundles_->lock(), 
                       "TableBasedRouter::reroute_all_bundles");

    log_debug("reroute_all_bundles... %zu bundles on pending list",
              pending_bundles_->size());

    // XXX/demmer this should cancel previous scheduled transmissions
    // if any decisions have changed

    BundleList::iterator iter;
    for (iter = pending_bundles_->begin();
         iter != pending_bundles_->end();
         ++iter)
    {
        route_bundle(*iter);
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::recompute_routes()
{
    reroute_all_bundles();
}

//----------------------------------------------------------------------
TableBasedRouter::DeferredList::DeferredList(const char* logpath,
                                             const LinkRef& link)
    : RouterInfo(),
      Logger("%s/deferred/%s", logpath, link->name()),
      list_(link->name_str() + ":deferred"),
      count_(0)
{
}

//----------------------------------------------------------------------
void
TableBasedRouter::DeferredList::dump_stats(oasys::StringBuffer* buf)
{
    buf->appendf(" -- %zu bundles_deferred", count_);
}

//----------------------------------------------------------------------
bool
TableBasedRouter::DeferredList::find(const BundleRef& bundle,
                                     ForwardingInfo* info)
{
    InfoMap::const_iterator iter = info_.find(bundle->bundleid());
    if (iter == info_.end()) {
        return false;
    }
    *info = iter->second;
    return true;
}

//----------------------------------------------------------------------
const ForwardingInfo&
TableBasedRouter::DeferredList::info(const BundleRef& bundle)
{
    InfoMap::const_iterator iter = info_.find(bundle->bundleid());
    ASSERT(iter != info_.end());
    return iter->second;
}

//----------------------------------------------------------------------
bool
TableBasedRouter::DeferredList::add(const BundleRef&      bundle,
                                    const ForwardingInfo& info)
{
    if (list_.contains(bundle)) {
        log_err("bundle *%p already in deferred list!",
                bundle.object());
        return false;
    }
    
    log_debug("adding *%p to deferred (length %zu)",
              bundle.object(), count_);

    count_++;
    list_.push_back(bundle);

    info_.insert(InfoMap::value_type(bundle->bundleid(), info));

    return true;
}

//----------------------------------------------------------------------
bool
TableBasedRouter::DeferredList::del(const BundleRef& bundle)
{
    if (! list_.erase(bundle)) {
        return false;
    }
    
    ASSERT(count_ > 0);
    count_--;
    
    log_debug("removed *%p from deferred (length %zu)",
              bundle.object(), count_);

    size_t n = info_.erase(bundle->bundleid());
    ASSERT(n == 1);
    
    return true;
}

//----------------------------------------------------------------------
TableBasedRouter::DeferredList*
TableBasedRouter::deferred_list(const LinkRef& link)
{
    DeferredList* dq = dynamic_cast<DeferredList*>(link->router_info());
    ASSERT(dq != NULL);
    return dq;
}

} // namespace dtn

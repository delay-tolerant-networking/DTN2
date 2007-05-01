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
#  include <config.h>
#endif

#include "TableBasedRouter.h"
#include "RouteTable.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleDaemon.h"
#include "contacts/Contact.h"
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
    reroute_all_bundles();        
}

//----------------------------------------------------------------------
void
TableBasedRouter::del_route(const EndpointIDPattern& dest)
{
    route_table_->del_entries(dest);
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
        return;
    }
    
    fwd_to_matching(bundle);
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_bundle_transmitted(BundleTransmittedEvent* event)
{
    Bundle* bundle = event->bundleref_.object();
    (void)bundle;
    log_debug("handle bundle transmitted: *%p", bundle);

    // XXX/demmer this should fix up the retention constraints to no
    // longer require that the bundle be kept
}
    
//----------------------------------------------------------------------
/*void
TableBasedRouter::handle_bundle_transmit_failed(BundleTransmitFailedEvent* event)
{
    Bundle* bundle = event->bundleref_.object();
    log_debug("handle bundle transmit failed: *%p", bundle);
    fwd_to_matching(bundle);
}*/

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
    // if we're configured to do so, create a route entry for the eid
    // specified by the link when it connected. if it's a dtn://xyz
    // URI that doesn't have anything following the "hostname" part,
    // we add a wildcard of '/*' to match all service tags.
    
    EndpointID eid = link->remote_eid();
    std::string eid_str = eid.str();
    
    if (config_.add_nexthop_routes_ && eid != EndpointID::NULL_EID())
    { 
        if (eid.scheme_str() == "dtn" &&
            eid.ssp().length() > 2 &&
            eid.ssp()[0] == '/' &&
            eid.ssp()[1] == '/' &&
            eid.ssp().find('/', 2) == std::string::npos)
        {
            eid_str += std::string("/*");
        }

        EndpointIDPattern eid_pattern(eid_str);

        RouteEntryVec ignored;
        if (route_table_->get_matching(eid_pattern, link, &ignored) == 0) {
            RouteEntry *entry = new RouteEntry(eid_pattern, link);
            entry->action_ = ForwardingInfo::FORWARD_ACTION;
            add_route(entry);
        }
    }
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
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_link_available(LinkAvailableEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    check_next_hop(link);
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_link_created(LinkCreatedEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    add_nexthop_route(link);
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_link_deleted(LinkDeletedEvent* event)
{
    LinkRef link = event->link_;
    ASSERT(link != NULL);
    ASSERT(link->isdeleted());

    route_table_->del_entries_for_nexthop(link);
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
    fwd_to_matching(event->bundle_.object());
}

//----------------------------------------------------------------------
void
TableBasedRouter::get_routing_state(oasys::StringBuffer* buf)
{
    EndpointIDVector long_eids;
    buf->appendf("Route table for %s router:\n\n", name_.c_str());
    route_table_->dump(buf, &long_eids);

    if (long_eids.size() > 0) {
        buf->appendf("\nLong Endpoint IDs referenced above:\n");
        for (u_int i = 0; i < long_eids.size(); ++i) {
            buf->appendf("\t[%d]: %s\n", i, long_eids[i].c_str());
        }
        buf->appendf("\n");
    }
    
    buf->append("\nClass of Service (COS) bits:\n"
                "\tB: Bulk  N: Normal  E: Expedited\n\n");
}

//----------------------------------------------------------------------
void
TableBasedRouter::tcl_dump_state(oasys::StringBuffer* buf)
{
    buf->append("{");

    oasys::ScopeLock l(route_table_->lock(),
                       "TableBasedRouter::tcl_dump_state");

    RouteEntryVec::const_iterator iter;
    for (iter = route_table_->route_table()->begin();
         iter != route_table_->route_table()->end(); ++iter)
    {
        const RouteEntry* e = *iter;
        buf->appendf("{%s %s source_eid %s priority %d}",
                     e->dest_pattern_.c_str(),
                     e->next_hop_->name(),
                     e->source_pattern_.c_str(),
                     e->route_priority_);
    }
    
    buf->append("}");
}

//----------------------------------------------------------------------
bool
TableBasedRouter::fwd_to_nexthop(Bundle* bundle, RouteEntry* route)
{
    LinkRef link = route->next_hop_;

    // if the link is open and not busy, send the bundle to it
    if (link->isopen() && !link->isbusy()) {
        log_debug("sending *%p to *%p", bundle, link.object());
        actions_->send_bundle(bundle, link, route->action(),
                              route->custody_timeout());
        return true;
    }

    // otherwise we can't send the bundle now, so add a forwarding log
    // entry indicating that transmission is pending on the given link
    //
    // XXX/demmer should also queue it on the link?
    //
    ForwardingInfo::state_t state = bundle->fwdlog_.get_latest_entry(link);
    if (state != ForwardingInfo::TRANSMIT_PENDING) {
        bundle->fwdlog_.add_entry(link, route->action(),
                                  ForwardingInfo::TRANSMIT_PENDING,
                                  route->custody_timeout());
    }
    
    // if the link is available and not open, open it
    if (link->isavailable() && (!link->isopen()) && (!link->isopening())) {
        log_debug("opening *%p because a message is intended for it",
                  link.object());
        actions_->open_link(link);
    }

    // otherwise, we can't do anything, so just log a bundle of
    // reasons why
    else {
        if (!link->isavailable()) {
            log_debug("can't forward *%p to *%p because link not available",
                      bundle, link.object());
        } else if (! link->isopen()) {
            log_debug("can't forward *%p to *%p because link not open",
                      bundle, link.object());
        } else if (link->isbusy()) {
            log_debug("can't forward *%p to *%p because link is busy",
                      bundle, link.object());
        } else {
            log_debug("can't forward *%p to *%p", bundle, link.object());
        }
    }

    return false;
}

//----------------------------------------------------------------------
bool
TableBasedRouter::should_fwd(const Bundle* bundle, RouteEntry* route)
{
    ForwardingInfo info;
    const LinkRef& link = route->next_hop_;
    bool found = bundle->fwdlog_.get_latest_entry(link, &info);

    if (found) {
        ASSERT(info.state() != ForwardingInfo::NONE);
    } else {
        ASSERT(info.state() == ForwardingInfo::NONE);
    }

    // check if we've already sent or are in the process of sending
    // the bundle on this link
    if (info.state() == ForwardingInfo::TRANSMITTED ||
        info.state() == ForwardingInfo::IN_FLIGHT)
    {
        log_debug("should_fwd bundle %d: "
                  "skip %s due to forwarding log entry %s",
                  bundle->bundleid_, link->name(),
                  ForwardingInfo::state_to_str(info.state()));
        return false;
    }

    // check if we're trying to send it right back where it came from
    // (and we know something about the remote eid)
    if (link->remote_eid() != EndpointID::NULL_EID() &&
        link->remote_eid() == bundle->prevhop_)
    {
        log_debug("should_fwd bundle %d: "
                  "skip %s since remote eid %s == bundle prevhop",
                  bundle->bundleid_, link->name(), link->remote_eid().c_str());
        return false;
    }

    // check if we've already sent the bundle to the node via some
    // other link
    size_t count = bundle->fwdlog_.get_count(
        link->remote_eid(),
        ForwardingInfo::TRANSMITTED | ForwardingInfo::IN_FLIGHT);
    
    if (count > 0)
    {
        log_debug("should_fwd bundle %d: "
                  "skip %s since already sent %zu times to remote eid %s",
                  bundle->bundleid_, link->name(),
                  count, link->remote_eid().c_str());
        return false;
    }

    // if the bundle has a a singleton destination endpoint, then
    // check if we already forwarded it somewhere else. if so, we
    // shouldn't forward it again
    if (bundle->singleton_dest_ &&
        route->action_ == ForwardingInfo::FORWARD_ACTION)
    {
        size_t count = bundle->fwdlog_.get_count(
            ForwardingInfo::TRANSMITTED | ForwardingInfo::IN_FLIGHT,
            ForwardingInfo::FORWARD_ACTION);
        
        if (count > 0) {
            log_debug("should_fwd bundle %d: "
                      "skip %s since already transmitted (count %zu)",
                      bundle->bundleid_, link->name(), count);
            return false;
        } else {
            log_debug("should_fwd bundle %d: "
                      "link %s ok since transmission count=%zu",
                      bundle->bundleid_, link->name(), count);
        }
    }

    // otherwise log the reason why we should send it
    log_debug("should_fwd bundle %d: "
              "match %s: forwarding log entry %s",
              bundle->bundleid_, link->name(),
              ForwardingInfo::state_to_str(info.state()));
    
    return true;
}

//----------------------------------------------------------------------
int
TableBasedRouter::fwd_to_matching(Bundle* bundle, const LinkRef& this_link_only)
{
    RouteEntryVec matches;
    RouteEntryVec::iterator iter;

    log_debug("fwd_to_matching: checking bundle %d, link %s",
              bundle->bundleid_,
              this_link_only == NULL ? "(any link)" : this_link_only->name());

    // XXX/demmer fix this
    if (bundle->owner_ == "DO_NOT_FORWARD") {
        log_notice("fwd_to_matching: "
                   "ignoring bundle %d since owner is DO_NOT_FORWARD",
                   bundle->bundleid_);
        return 0;
    }

    // get_matching only returns results that match the this_link_only
    // link, if it's not null
    route_table_->get_matching(bundle->dest_, this_link_only, &matches);

    // sort the matching routes by priority, allowing subclasses to
    // override the way in which the sorting occurs
    sort_routes(bundle, &matches);

    log_debug("fwd_to_matching bundle id %d: checking %zu route entry matches",
              bundle->bundleid_, matches.size());
    
    unsigned int count = 0;
    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        if (! should_fwd(bundle, *iter)) {
            continue;
        }
        
        if (!fwd_to_nexthop(bundle, *iter)) {
            continue;
        }
        
        ++count;
    }

    log_debug("fwd_to_matching bundle id %d: forwarded on %u links",
              bundle->bundleid_, count);
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
    log_debug("check_next_hop %s -> %s: checking pending bundle list...",
              next_hop->name(), next_hop->nexthop());

    // when the link comes up or is made available, any bundles
    // destined for it will have a transmit pending entry in the
    // forwarding log
    //
    // XXX/demmer this would be easier if they were just on the link
    // queue...
    oasys::ScopeLock l(pending_bundles_->lock(), 
                       "TableBasedRouter::check_next_hop");
    BundleList::iterator iter;
    for (iter = pending_bundles_->begin();
         iter != pending_bundles_->end();
         ++iter)
    {
        if (next_hop->isbusy()) {
            log_debug("check_next_hop %s: link is busy, stopping loop",
                      next_hop->name());
            break;
        }
        
        BundleRef bundle("TableBasedRouter::check_next_hop");
        bundle = *iter;
        
        ForwardingInfo info;
        bool ok = bundle->fwdlog_.get_latest_entry(next_hop, &info);

        if (ok && (info.state() == ForwardingInfo::TRANSMIT_PENDING)) {
            // if the link is available and not open, open it
            if (next_hop->isavailable() &&
                (!next_hop->isopen()) && (!next_hop->isopening()))
            {
                log_debug("check_next_hop: "
                          "opening *%p because a message is intended for it",
                          next_hop.object());
                actions_->open_link(next_hop);
            }
            
            log_debug("check_next_hop: sending *%p to *%p",
                      bundle.object(), next_hop.object());
            actions_->send_bundle(bundle.object() , next_hop,
                                  info.action(), info.custody_spec());
        }
    }
}

//----------------------------------------------------------------------
void
TableBasedRouter::reroute_all_bundles()
{
    oasys::ScopeLock l(pending_bundles_->lock(), 
                       "TableBasedRouter::check_next_hop");

    log_debug("reroute_all_bundles... %zu bundles on pending list",
              pending_bundles_->size());

    BundleList::iterator iter;
    for (iter = pending_bundles_->begin();
         iter != pending_bundles_->end();
         ++iter)
    {
        fwd_to_matching(*iter);
    }
}

} // namespace dtn

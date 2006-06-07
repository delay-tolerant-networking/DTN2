/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
    : BundleRouter(classname, name)
{
    route_table_ = new RouteTable(name);
}

//----------------------------------------------------------------------
void
TableBasedRouter::add_route(RouteEntry *entry)
{
    route_table_->add_entry(entry);
    check_next_hop(entry->next_hop_);        
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
    fwd_to_matching(bundle);
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_bundle_transmit_failed(BundleTransmitFailedEvent* event)
{
    Bundle* bundle = event->bundleref_.object();
    log_debug("handle bundle transmit failed: *%p", bundle);
    fwd_to_matching(bundle);
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
TableBasedRouter::handle_contact_up(ContactUpEvent* event)
{
    check_next_hop(event->contact_->link());
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_link_available(LinkAvailableEvent* event)
{
    check_next_hop(event->link_);
}

//----------------------------------------------------------------------
void
TableBasedRouter::handle_link_created(LinkCreatedEvent* event)
{
    Link* link = event->link_;
    EndpointID eid = link->remote_eid();
    if (! eid.equals(EndpointID::NULL_EID()) ) {
        // create route entry, post new route event
        RouteEntry *entry = new RouteEntry(
                EndpointIDPattern(eid.str() + std::string("/*")), link);
        entry->action_ = FORWARD_UNIQUE;
        BundleDaemon::post(new RouteAddEvent(entry));
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
    fwd_to_matching(event->bundle_.object());
}

//----------------------------------------------------------------------
void
TableBasedRouter::get_routing_state(oasys::StringBuffer* buf)
{
    buf->appendf("Route table for %s router:\n", name_.c_str());
    route_table_->dump(buf);
}

//----------------------------------------------------------------------
void
TableBasedRouter::fwd_to_nexthop(Bundle* bundle, RouteEntry* route)
{
    Link* link = route->next_hop_;

    // if the link is open and not busy, send the bundle to it
    if (link->isopen() && !link->isbusy()) {
        log_debug("sending *%p to *%p", bundle, link);
        actions_->send_bundle(bundle, link,
                              route->action_, route->custody_timeout_);
    }

    // if the link is available and not open, open it
    else if (link->isavailable() && (!link->isopen()) && (!link->isopening())) {
        log_debug("opening *%p because a message is intended for it", link);
        actions_->open_link(link);
    }

    // otherwise, we can't do anything, so just log a bundle of
    // reasons why
    else {
        if (!link->isavailable()) {
            log_debug("can't forward *%p to *%p because link not available",
                      bundle, link);
        } else if (! link->isopen()) {
            log_debug("can't forward *%p to *%p because link not open",
                      bundle, link);
        } else if (link->isbusy()) {
            log_debug("can't forward *%p to *%p because link is busy",
                      bundle, link);
        } else {
            log_debug("can't forward *%p to *%p", bundle, link);
        }
    }
}

//----------------------------------------------------------------------
bool
TableBasedRouter::should_fwd(const Bundle* bundle, RouteEntry* route)
{
    ForwardingInfo info;
    bool found = bundle->fwdlog_.get_latest_entry(route->next_hop_, &info);

    if (found) {
        ASSERT(info.state_ != ForwardingInfo::NONE);
    } else {
        ASSERT(info.state_ == ForwardingInfo::NONE);
    }
    
    if (info.state_ == ForwardingInfo::TRANSMITTED ||
        info.state_ == ForwardingInfo::IN_FLIGHT)
    {
        log_debug("should_fwd bundle %d: "
                  "skip %s due to forwarding log entry %s",
                  bundle->bundleid_, route->next_hop_->name(),
                  ForwardingInfo::state_to_str(info.state_));
        return false;
    }

    if (route->action_ == FORWARD_UNIQUE) {
        size_t count;
        
        count = bundle->fwdlog_.get_transmission_count(FORWARD_UNIQUE, true);
        if (count > 0) {
            log_debug("should_fwd bundle %d: "
                      "skip %s since already transmitted (count %zu)",
                      bundle->bundleid_, route->next_hop_->name(), count);
            return false;
        } else {
            log_debug("should_fwd bundle %d: "
                      "link %s ok since transmission count=%d",
                      bundle->bundleid_, route->next_hop_->name(), count);
        }
    }
    
    if (info.state_ == ForwardingInfo::TRANSMIT_FAILED) {
        log_debug("should_fwd bundle %d: "
                  "match %s: forwarding log entry %s TRANSMIT_FAILED %d",
                  bundle->bundleid_, route->next_hop_->name(),
                  ForwardingInfo::state_to_str(info.state_),
                  bundle->bundleid_);
        
    } else {
        log_debug("should_fwd bundle %d: "
                  "match %s: forwarding log entry %s",
                  bundle->bundleid_, route->next_hop_->name(),
                  ForwardingInfo::state_to_str(info.state_));
    }

    return true;
}

//----------------------------------------------------------------------
int
TableBasedRouter::fwd_to_matching(Bundle* bundle, Link* next_hop)
{
    RouteEntryVec matches;
    RouteEntryVec::iterator iter;

    // get_matching only returns results that match the next_hop link,
    // if it's not null
    route_table_->get_matching(bundle->dest_, next_hop, &matches);

    // sort the list by route priority, breaking ties with the total
    // bytes in flight
    matches.sort_by_priority();
    
    int count = 0;
    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        if (! should_fwd(bundle, *iter)) {
            continue;
        }
        
        fwd_to_nexthop(bundle, *iter);
        ++count;
    }

    log_debug("fwd_to_matching bundle id %d: %d matches",
              bundle->bundleid_, count);
    return count;
}

//----------------------------------------------------------------------
void
TableBasedRouter::check_next_hop(Link* next_hop)
{
    log_debug("check_next_hop %s: checking pending bundle list...",
              next_hop->nexthop());

    oasys::ScopeLock l(pending_bundles_->lock(), 
                       "TableBasedRouter::check_next_hop");
    BundleList::iterator iter;
    for (iter = pending_bundles_->begin();
         iter != pending_bundles_->end();
         ++iter)
    {
        fwd_to_matching(*iter, next_hop);
    }
}

} // namespace dtn

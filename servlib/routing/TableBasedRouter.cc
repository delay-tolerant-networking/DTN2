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
#include "bundling/BundleConsumer.h"
#include "bundling/BundleDaemon.h"
#include "bundling/FragmentManager.h"
#include "contacts/Contact.h"
#include "contacts/Link.h"

namespace dtn {

TableBasedRouter::TableBasedRouter(const std::string& name)
    : BundleRouter(name)
{
    route_table_ = new RouteTable(name);
}

void
TableBasedRouter::add_route(RouteEntry *entry)
{
    route_table_->add_entry(entry);
    check_next_hop(entry->next_hop_);        
}

/**
 * Event handler overridden from BundleRouter / BundleEventHandler
 * that dispatches to the type specific handlers where
 * appropriate.
 */
void
TableBasedRouter::handle_event(BundleEvent* event)
{
    dispatch_event(event);
}

/**
 * Handler for new bundle arrivals simply forwards the newly arrived
 * bundle to all matching entries in the table.
 */
void
TableBasedRouter::handle_bundle_received(BundleReceivedEvent* event)
{
    Bundle* bundle = event->bundleref_.object();
    log_debug("handle bundle received: *%p", bundle);
    fwd_to_matching(bundle);
}

/**
 * Default event handler when a new route is added by the command
 * or management interface.
 *
 * Adds an entry to the route table, then walks the pending list
 * to see if any bundles match the new route.
 */
void
TableBasedRouter::handle_route_add(RouteAddEvent* event)
{
    add_route(event->entry_);
}

/**
 * When a contact comes up, check to see if there are any matching
 * bundles for it.
 */
void
TableBasedRouter::handle_contact_up(ContactUpEvent* event)
{
    check_next_hop(event->contact_->link());
}

/**
 * Ditto if a link becomes available.
 */
void
TableBasedRouter::handle_link_available(LinkAvailableEvent* event)
{
    check_next_hop(event->link_);
}

/**
 * Format the given StringBuffer with current routing info.
 */
void
TableBasedRouter::get_routing_state(oasys::StringBuffer* buf)
{
    buf->appendf("Route table for %s router:\n", name_.c_str());
    route_table_->dump(buf);
}

/**
 * Add an action to forward a bundle to a next hop route, making
 * sure to do reassembly if the forwarding action specifies as
 * such.
 */
void
TableBasedRouter::fwd_to_nexthop(Bundle* bundle, RouteEntry* nexthop)
{
    Link* link = nexthop->next_hop_;

    // if the link is open and idle, send the bundle to it
    if (link->isopen() && !link->isbusy()) {
        log_debug("sending *%p to *%p", bundle, link);
        actions_->send_bundle(bundle, link);
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

/**
 * Check the route table entries that match the given bundle and
 * have not already been found in the bundle history. If a match
 * is found, call fwd_to_nexthop on it.
 *
 * @param bundle	the bundle to forward
 * @param next_hop	if specified, restricts forwarding to the given
 * 			next hop link
 *
 * Returns the number of matches found and assigned.
 */
int
TableBasedRouter::fwd_to_matching(Bundle* bundle, Link* next_hop)
{
    RouteEntrySet matches;
    RouteEntrySet::iterator iter;

    route_table_->get_matching(bundle->dest_, &matches);
    
    int count = 0;
    for (iter = matches.begin(); iter != matches.end(); ++iter)
    {
        if (next_hop != NULL && (next_hop != (*iter)->next_hop_)) {
            log_debug("fwd_to_matching %s: "
                      "ignoring match %s since next_hop link %s set",
                      bundle->dest_.c_str(), (*iter)->next_hop_->name(),
                      next_hop->name());
            continue;
        }

        ForwardingEntry::state_t state =
            bundle->fwdlog_.get_state((*iter)->next_hop_->nexthop());
        
        if (state == ForwardingEntry::SENT ||
            state == ForwardingEntry::IN_FLIGHT)
        {
            log_debug("fwd_to_matching %s: "
                      "ignore match %s due to forwarding log entry %s",
                      bundle->dest_.c_str(), (*iter)->next_hop_->name(),
                      ForwardingEntry::state_to_str(state));
            continue;
        }
                      
        fwd_to_nexthop(bundle, *iter);
        ++count;
    }

    log_debug("fwd_to_matching %s: %d matches", bundle->dest_.c_str(), count);
    return count;
}

/**
 * Called whenever a new consumer (i.e. registration or contact)
 * arrives. This walks the list of all pending bundles, forwarding
 * all matches to the new contact.
 */
void
TableBasedRouter::check_next_hop(Link* next_hop)
{
    log_debug("check_next_hop %s: checking pending bundle list...",
              next_hop->dest_str());

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

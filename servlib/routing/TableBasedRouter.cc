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
#include "bundling/Link.h"

namespace dtn {

TableBasedRouter::TableBasedRouter(const std::string& name)
    : BundleRouter(name)
{    
    route_table_ = new RouteTable();
}

void
TableBasedRouter::add_route(RouteEntry *entry)
{
    route_table_->add_entry(entry);
    new_next_hop(entry->pattern_, entry->next_hop_);        
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
    Bundle* bundle = event->bundleref_.bundle();
    log_debug("handle bundle received: *%p", bundle);
    fwd_to_matching(bundle, true);
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
    // If nexthop->nexthop_ is a Link
    // and if it is available open it
    BundleConsumer* bc = nexthop->next_hop_;

    if (bc->type() == BundleConsumer::LINK)
    {
        Link* link = (Link *)bc;
        if ((link->type() == Link::ONDEMAND) &&
            link->isavailable() && (!link->isopen()))
        {
            log_info("Opening ONDEMAND link %s because messages are queued for it",
                     link->name());

            actions_->open_link(link);
        }
    }

    // Add message to the next hop queue
    actions_->enqueue_bundle(bundle, nexthop->next_hop_,
                             nexthop->action_, nexthop->mapping_grp_);
}

/**
 * Get all matching entries from the routing table, and add a
 * corresponding forwarding action to the action list.
 *
 * Note that if the include_local flag is set, then local routes
 * (i.e. registrations) are included in the list.
 *
 * Returns the number of matches found and assigned.
 */
int
TableBasedRouter::fwd_to_matching(Bundle* bundle, bool include_local)
{
    RouteEntry* entry;
    RouteEntrySet matches;
    RouteEntrySet::iterator iter;

    route_table_->get_matching(bundle->dest_, &matches);
    
    int count = 0;
    for (iter = matches.begin(); iter != matches.end(); ++iter) {
        entry = *iter;

        if ((include_local == false) && entry->next_hop_->is_local())
            continue;

        fwd_to_nexthop(bundle, entry);
        
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
TableBasedRouter::new_next_hop(const BundleTuplePattern& dest,
                               BundleConsumer* next_hop)
{
    log_debug("new_next_hop %s: checking pending bundle list...",
              next_hop->dest_str());
    BundleList::iterator iter;

    oasys::ScopeLock l(pending_bundles_->lock());
    
    for (iter = pending_bundles_->begin();
         iter != pending_bundles_->end();
         ++iter)
    {
        fwd_to_matching(*iter, true);
    }
}

} // namespace dtn

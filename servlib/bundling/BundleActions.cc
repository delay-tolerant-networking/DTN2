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

#include "BundleActions.h"
#include "Bundle.h"
#include "BundleConsumer.h"
#include "BundleList.h"
#include "storage/BundleStore.h"
#include "Link.h"

namespace dtn {

/**
 * Queue a bundle for delivery on the given next hop (i.e. a
 * contact, peer, registration, etc). In queueing the bundle for
 * delivery, this creates a new BundleMapping to store any router
 * state about this decision.
 *
 * @param bundle	the bundle
 * @param nextho	the next hop consumer
 * @param fwdaction	action type code to be returned in the
 *                      BUNDLE_TRANSMITTED event
 * @param mapping_grp	group identifier for the set of mappings
 * @param expiration	expiration time for this mapping
 * @param router_info	opaque slot for associated routing info
 */
void
BundleActions::enqueue_bundle(Bundle* bundle, BundleConsumer* nexthop,
                              bundle_fwd_action_t fwdaction,
                              int mapping_grp, u_int32_t expiration,
                              RouterInfo* router_info)
{
    log_debug("enqueue bundle %d on next hop %s (type %s)",
              bundle->bundleid_, nexthop->dest_str(), nexthop->type_str());

    // XXX/demmer handle reassembly
    ASSERT(fwdaction != FORWARD_REASSEMBLE);

    BundleMapping mapping(fwdaction, mapping_grp, expiration, router_info);

    if (nexthop->is_queued(bundle)) {
        log_warn("bundle %d already queued on next hop %s, ignoring duplicate",
                 bundle->bundleid_, nexthop->dest_str());
    } else {
        nexthop->consume_bundle(bundle, &mapping);
    }
}

/**
 * Remove a previously queued bundle.
 *
 * @param bundle	the bundle
 * @param nexthop	the next hop consumer
 * @return              true if successful
 */
bool
BundleActions::dequeue_bundle(Bundle* bundle, BundleConsumer* nexthop)
{
    log_debug("dequeue bundle %d from next hop %s (type %s)",
              bundle->bundleid_, nexthop->dest_str(), nexthop->type_str());

    if (!nexthop->is_queued(bundle)) {
        log_warn("bundle %d not queued on next hop %s",
                 bundle->bundleid_, nexthop->dest_str());
        return false;
    } else {
        // XXX/demmer what about ref count?
        nexthop->dequeue_bundle(bundle, NULL);
        return true;
    }
}

/**
 * Move the all queued bundles from one consumer to another.
 */
void
BundleActions::move_contents(BundleConsumer* source, BundleConsumer* dest)
{
    // XXX/demmer get rid of this whole fn
    BundleList* src_list = source->bundle_list();
    
    log_debug("moving %u bundles from from next hop %s (type %s) "
              "to next hop %s (type %s)",
              (u_int)src_list->size(), source->dest_str(), source->type_str(),
              dest->dest_str(), dest->type_str());

    // we don't use BundleList::move_contents since really we want to
    // call consume_bundle for each, not the vanilla push_back
    Bundle* b;
    BundleMapping* m;
    do {
        b = src_list->pop_front(&m);
        if (b) {
            dest->consume_bundle(b, m); // copies m
            delete m;
        }
    } while (b != NULL);
}

/**
 * Add the given bundle to the data store.
 */
void
BundleActions::store_add(Bundle* bundle)
{
    log_debug("adding bundle %d to data store", bundle->bundleid_);
    bool added = BundleStore::instance()->add(bundle);
    ASSERT(added);
}

/**
 * Remove the given bundle from the data store.
 */
void
BundleActions::store_del(Bundle* bundle)
{
    log_debug("removing bundle %d from data store", bundle->bundleid_);
    bool removed = BundleStore::instance()->del(bundle->bundleid_);
    ASSERT(removed);
}

/**
 * Open a link for bundle transmission.
 */
void
BundleActions::open_link(Link* link)
{
    log_debug("opening link %s", link->name());
    if (link->isopen()) {
        log_warn("not opening link %s since already open", link->name());
    } else {
        link->open();
    }
}

/**
 * Close the given link.
 */
void
BundleActions::close_link(Link* link)
{
    log_debug("closing link %s", link->name());
    if (! link->isopen()) {
        log_warn("not closing link %s since already closed", link->name());
    } else {
        link->close();
    }
}

} // namespace dtn

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

#include "BundleActions.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleList.h"
#include "conv_layers/ConvergenceLayer.h"
#include "contacts/Link.h"
#include "storage/BundleStore.h"

namespace dtn {

//----------------------------------------------------------------------
void
BundleActions::open_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    if (link->isdeleted()) {
        log_debug("BundleActions::open_link: "
                  "cannot open deleted link %s", link->name());
        return;
    }

    oasys::ScopeLock l(link->lock(), "BundleActions::open_link");

    if (link->isopen() || link->contact() != NULL) {
        log_err("not opening link %s since already open", link->name());
        return;
    }

    if (! link->isavailable()) {
        log_err("not opening link %s since not available", link->name());
        return;
    }
    
    log_debug("BundleActions::open_link: opening link %s", link->name());

    link->open();
}

//----------------------------------------------------------------------
void
BundleActions::close_link(const LinkRef& link)
{
    ASSERT(link != NULL);

    if (! link->isopen() && ! link->isopening()) {
        log_err("not closing link %s since not open", link->name());
        return;
    }

    log_debug("BundleActions::close_link: closing link %s", link->name());

    link->close();
    ASSERT(link->contact() == NULL);
}

//----------------------------------------------------------------------
bool
BundleActions::queue_bundle(Bundle* bundle, const LinkRef& link,
                            ForwardingInfo::action_t action,
                            const CustodyTimerSpec& custody_timer)
{
    BundleRef bref(bundle, "BundleActions::queue_bundle");
    
    ASSERT(link != NULL);
    if (link->isdeleted()) {
        log_warn("BundleActions::queue_bundle: "
                 "failed to send bundle *%p on link %s",
                 bundle, link->name());
        return false;
    }
    
    log_debug("trying to find xmit blocks for bundle id:%d on link %s",
              bundle->bundleid_, link->name());

    if (bundle->xmit_blocks_.find_blocks(link) != NULL) {
        log_err("BundleActions::queue_bundle: "
                "link not ready to handle bundle (block vector already exists), "
                "dropping send request");
        return false;
    }

    // XXX/demmer this should be moved somewhere in the router
    // interface so it can select options for the outgoing bundle
    // blocks (e.g. security)
    // XXX/ngoffee It's true the router should be able to select
    // blocks for various purposes, but I'd like the security policy
    // checks and subsequent block selection to remain inside the BPA,
    // with the DP pushing (firewall-like) policies and keys down via
    // a PF_KEY-like interface.
    log_debug("trying to create xmit blocks for bundle id:%d on link %s",
              bundle->bundleid_, link->name());
    BlockInfoVec* blocks = BundleProtocol::prepare_blocks(bundle, link);
    size_t total_len = BundleProtocol::generate_blocks(bundle, blocks, link);

    log_debug("queue bundle *%p on %s link %s (%s) (total len %zu)",
              bundle, link->type_str(), link->name(), link->nexthop(),
              total_len);

    ForwardingInfo::state_t state = bundle->fwdlog_.get_latest_entry(link);
    if (state == ForwardingInfo::IN_FLIGHT) {
        log_err("queue bundle *%p on %s link %s (%s): already in flight",
                bundle, link->type_str(), link->name(), link->nexthop());
        return false;
    }

    if ((link->params().mtu_ != 0) && (total_len > link->params().mtu_)) {
        log_err("queue bundle *%p on %s link %s (%s): length %zu > mtu %u",
                bundle, link->type_str(), link->name(), link->nexthop(),
                total_len, link->params().mtu_);
        return false;
    }

    // Make sure that the bundle isn't unexpectedly already on the
    // queue or in flight on the link
    if (link->queue()->contains(bundle))
    {
        log_err("queue bundle *%p on link *%p: already queued on link",
                bundle, link.object());
        return false;
    }

    if (link->inflight()->contains(bundle))
    {
        log_err("queue bundle *%p on link *%p: already in flight on link",
                bundle, link.object());
        return false;
    }

    // But, the bundle might have been deferred on the link, so remove
    // it from that queue.
    if (link->del_from_deferred(bref))
    {
        log_debug("queue bundle removed *%p from link *%p deferred list",
                  bundle, link.object());
    }
    
    log_debug("adding forward log entry for %s link %s "
              "with nexthop %s and remote eid %s to *%p",
              link->type_str(), link->name(),
              link->nexthop(), link->remote_eid().c_str(), bundle);
    
    bundle->fwdlog_.add_entry(link, action, ForwardingInfo::IN_FLIGHT,
                              custody_timer);

    log_debug("adding *%p to link %s's queue (length %u)",
              bundle, link->name(), link->stats_.bundles_queued_);

    link->add_to_queue(bref, total_len);
    
    // finally, kick the convergence layer
    link->clayer()->bundle_queued(link, bref);
    
    return true;
}

//----------------------------------------------------------------------
void
BundleActions::cancel_bundle(Bundle* bundle, const LinkRef& link)
{
    BundleRef bref(bundle, "BundleActions::cancel_bundle");
    
    ASSERT(link != NULL);
    if (link->isdeleted()) {
        log_debug("BundleActions::cancel_bundle: "
                  "cannot cancel bundle on deleted link %s", link->name());
        return;
    }

    log_debug("BundleActions::cancel_bundle: cancelling *%p on *%p",
              bundle, link.object());

    // Try to remove the bundle from the link's delayed-send queue and
    // deferred queue. If it's there, then we can safely remove it and
    // post the send cancelled request without involving the
    // convergence layer.
    //
    // If instead it's actually in flight on the link, then we call
    // down to the convergence layer to see if it can interrupt
    // transmission, in which case it's responsible for posting the
    // send cancelled event.
    
    BlockInfoVec* blocks = bundle->xmit_blocks_.find_blocks(link);
    
    if (link->del_from_deferred(bref)) {
        // sanity checks
        if (blocks != NULL) {
            log_err("BundleActions::cancel_bundle: "
                    "*%p queued on *%p deferred list but has xmit blocks!",
                    bundle, link.object());
        }
        if (link->inflight()->contains(bundle)) {
            log_err("BundleActions::cancel_bundle: "
                    "*%p queued on *%p deferred list and inflight list!",
                    bundle, link.object());
        }
        if (link->queue()->contains(bundle)) {
            log_err("BundleActions::cancel_bundle: "
                    "*%p queued on *%p deferred list and link queue",
                    bundle, link.object());
        }
        
        BundleDaemon::post(new BundleSendCancelledEvent(bundle, link));

    } else {
        if (blocks == NULL) {
            log_warn("BundleActions::cancel_bundle: "
                     "cancel *%p but no blocks queued or inflight on *%p",
                     bundle, link.object());
            return; 
        }
        
        size_t total_len = BundleProtocol::total_length(blocks);
        
        if (link->del_from_queue(bref, total_len)) {
            BundleDaemon::post(new BundleSendCancelledEvent(bundle, link));
            
        } else if (link->inflight()->contains(bundle)) {
            link->clayer()->cancel_bundle(link, bref);
        }
        else {
            log_warn("BundleActions::cancel_bundle: "
                     "cancel *%p but not deferred, queued, or inflight on *%p",
                     bundle, link.object());
        }
    }
}

//----------------------------------------------------------------------
void
BundleActions::inject_bundle(Bundle* bundle)
{
    PANIC("XXX/demmer fix inject bundle");
    
    log_debug("inject bundle *%p", bundle);
    BundleDaemon::instance()->pending_bundles()->push_back(bundle);
    store_add(bundle);
}

//----------------------------------------------------------------------
bool
BundleActions::delete_bundle(Bundle* bundle,
                             BundleProtocol::status_report_reason_t reason,
                             bool log_on_error)
{
    log_debug("attempting to delete bundle *%p from data store", bundle);
    bool del = BundleDaemon::instance()->delete_bundle(bundle, reason);

    if (log_on_error && !del) {
        log_err("Failed to delete bundle *%p from data store", bundle);
    }
    return del;
}

//----------------------------------------------------------------------
void
BundleActions::store_add(Bundle* bundle)
{
    log_debug("adding bundle %d to data store", bundle->bundleid_);
    bool added = BundleStore::instance()->add(bundle);
    if (! added) {
        log_crit("error adding bundle %d to data store!!", bundle->bundleid_);
    }
}

//----------------------------------------------------------------------
void
BundleActions::store_update(Bundle* bundle)
{
    log_debug("updating bundle %d in data store", bundle->bundleid_);
    bool updated = BundleStore::instance()->update(bundle);
    if (! updated) {
        log_crit("error updating bundle %d in data store!!", bundle->bundleid_);
    }
}

//----------------------------------------------------------------------
void
BundleActions::store_del(Bundle* bundle)
{
    log_debug("removing bundle %d from data store", bundle->bundleid_);
    bool removed = BundleStore::instance()->del(bundle);
    if (! removed) {
        log_crit("error removing bundle %d from data store!!",
                 bundle->bundleid_);
    }
}

} // namespace dtn

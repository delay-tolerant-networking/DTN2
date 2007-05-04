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
BundleActions::send_bundle(Bundle* bundle, const LinkRef& link,
                           ForwardingInfo::action_t action,
                           const CustodyTimerSpec& custody_timer)
{
    ASSERT(link != NULL);
    if (link->isdeleted()) {
        log_warn("BundleActions::send_bundle: "
                 "failed to send bundle *%p on link %s",
                 bundle, link->name());
        return false;
    }
    
    log_debug("trying to find xmit blocks for bundle id:%d on link %s",
              bundle->bundleid_,link->name());

    if (bundle->xmit_blocks_.find_blocks(link) != NULL) {
        log_err("BundleActions::send_bundle: "
                "link not ready to handle bundle (block vector already exists), "
                "dropping send request");
        return false;
    }

    // XXX/demmer this should be moved somewhere in the router
    // interface so it can select options for the outgoing bundle
    // blocks (e.g. security)
    log_debug("trying to create xmit blocks for bundle id:%d on link %s",
              bundle->bundleid_,link->name());
    BlockInfoVec* blocks = BundleProtocol::prepare_blocks(bundle, link);
    size_t total_len = BundleProtocol::generate_blocks(bundle, blocks, link);

    log_debug("send bundle *%p to %s link %s (%s) (total len %zu)",
              bundle, link->type_str(), link->name(), link->nexthop(),
              total_len);

    ForwardingInfo::state_t state = bundle->fwdlog_.get_latest_entry(link);
    if (state == ForwardingInfo::IN_FLIGHT) {
        log_err("send bundle *%p to %s link %s (%s): already in flight",
                bundle, link->type_str(), link->name(), link->nexthop());
        return false;
    }

    if ((link->params().mtu_ != 0) && (total_len > link->params().mtu_)) {
        log_err("send bundle *%p to %s link %s (%s): length %zu > mtu %u",
                bundle, link->type_str(), link->name(), link->nexthop(),
                total_len, link->params().mtu_);
        return false;
    }

    // Make sure that the bundle isn't unexpectedly already on the queue
    if (! link->clayer()->has_persistent_link_queues() &&
        link->queue()->contains(bundle))
    {
        log_err("send bundle *%p on link *%p: already queued on link",
                bundle, link.object());
        return false;
    }
    
    // XXX/demmer a better model would have the forwarding log be a
    // real append-only log of actions, and there would be no update()
    // interface. The job of get_count would be a bit more complicated
    // since it would only pay attention to the latest entries
    if (state == ForwardingInfo::TRANSMIT_PENDING) {
        log_debug("updating forward log entry for %s link %s "
                  "with nexthop %s and remote eid %s to *%p "
                  "from TRANSMIT_PENDING to IN_FLIGHT",
                  link->type_str(), link->name(),
                  link->nexthop(), link->remote_eid().c_str(), bundle);
        bundle->fwdlog_.update(link, ForwardingInfo::IN_FLIGHT);

    } else {
        log_debug("adding forward log entry for %s link %s "
                  "with nexthop %s and remote eid %s to *%p",
                  link->type_str(), link->name(),
                  link->nexthop(), link->remote_eid().c_str(), bundle);
    
        bundle->fwdlog_.add_entry(link, action, ForwardingInfo::IN_FLIGHT,
                                  custody_timer);
    }

    link->stats()->bundles_queued_++;
    link->stats()->bytes_queued_ += total_len;

    // If the link is open, tell the convergence layer to send the
    // bundle. Otherwise, we either queue the bundle or kick the
    // convergence layer to tell it to send the bundle on the down
    // link, depending on the CL behavior
    if (link->state() == Link::OPEN)
    {
        log_debug("immediate send of bundle *%p to link *%p: link in state %s",
                  bundle, link.object(), Link::state_to_str(link->state()));

        // XXX/demmer this is needed due to BBN's recent changes but
        // it seems like a better model would be to always call
        // send_bundle or send_bundle_on_down_link. then based on how
        // the CL itself operates, it either puts the bundle on the
        // queue or doesn't
        if (!link->clayer()->has_persistent_link_queues()) {
            link->queue()->push_back(bundle);
        }
        
        link->clayer()->send_bundle(link->contact(), bundle);
    }
    else
    {
        log_debug("delayed send of bundle *%p to link *%p: link in state %s",
                  bundle, link.object(), Link::state_to_str(link->state()));
        
        if (link->clayer()->has_persistent_link_queues()) {
            link->clayer()->send_bundle_on_down_link(link, bundle);
        } else {
            link->queue()->push_back(bundle);
        }
    }
    
    return true;
}

//----------------------------------------------------------------------
bool
BundleActions::cancel_bundle(Bundle* bundle, const LinkRef& link)
{
    ASSERT(link != NULL);
    if (link->isdeleted()) {
        log_debug("BundleActions::cancel_bundle: "
                  "cannot cancel bundle on deleted link %s", link->name());
        return false;
    }

    log_debug("BundleActions::cancel_bundle: "
              "cancel bundle *%p on %s link %s (%s)",
              bundle, link->type_str(), link->name(), link->nexthop());
              
    // Remove the bundle from the link's delayed-send queue
    if (link->queue()->erase(bundle, false))
    {
        log_info("removed delayed-send bundle id:%d from link %s queue",
                 bundle->bundleid_,
                 link->name());
    }

    // If that bundle is in flight on the link on the link, cancel it
    ForwardingInfo fwdinfo;
    bool ok = bundle->fwdlog_.get_latest_entry(link, &fwdinfo);
    if (ok && fwdinfo.state() == ForwardingInfo::IN_FLIGHT);
    {
        return link->clayer()->cancel_bundle(link, bundle);
    }
    
    return false;
}

//----------------------------------------------------------------------
void
BundleActions::inject_bundle(Bundle* bundle)
{
    PANIC("XXX/demmer fix this");
    
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

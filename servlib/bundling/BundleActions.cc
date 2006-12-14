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
BundleActions::open_link(Link* link)
{
    log_debug("opening link %s", link->name());

    if (link->isopen() || link->contact() != NULL) {
        log_err("not opening link %s since already open", link->name());
        return;
    }

    if (! link->isavailable()) {
        log_err("not opening link %s since not available", link->name());
        return;
    }
    
    link->open();
}

//----------------------------------------------------------------------
void
BundleActions::close_link(Link* link)
{
    log_debug("closing link %s", link->name());

    if (! link->isopen() && ! link->isopening()) {
        log_err("not closing link %s since not open", link->name());
        return;
    }

    link->close();
    ASSERT(link->contact() == NULL);
}

//----------------------------------------------------------------------
bool
BundleActions::send_bundle(Bundle* bundle, Link* link,
                           ForwardingInfo::action_t action,
                           const CustodyTimerSpec& custody_timer)
{
    if(bundle->xmit_blocks_.find_blocks(link) != NULL)
    {
        log_err("link not ready to handle another bundle, dropping send request");
        return false;
    }


    // XXX/demmer this should be moved somewhere in the router
    // interface so it can select options for the outgoing bundle
    // blocks (e.g. security)
    BlockInfoVec* blocks = BundleProtocol::prepare_blocks(bundle, link);
    size_t total_len = BundleProtocol::generate_blocks(bundle, blocks, link);

    log_debug("send bundle *%p to %s link %s (%s) (total len %zu)",
              bundle, link->type_str(), link->name(), link->nexthop(),
              total_len);
    
    if (link->state() != Link::OPEN) {
        log_err("send bundle *%p to %s link %s (%s): link not open!!",
                bundle, link->type_str(), link->name(), link->nexthop());
        return false;
    }

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

    bundle->fwdlog_.add_entry(link, action, ForwardingInfo::IN_FLIGHT,
                              custody_timer);

    link->stats()->bundles_queued_++;
    link->stats()->bytes_queued_ += total_len;
    
    ASSERT(link->contact() != NULL);
    link->clayer()->send_bundle(link->contact(), bundle);
    return true;
}

//----------------------------------------------------------------------
bool
BundleActions::cancel_bundle(Bundle* bundle, Link* link)
{
    log_debug("cancel bundle *%p on %s link %s (%s)",
              bundle, link->type_str(), link->name(), link->nexthop());

    if (link->state() != Link::OPEN) {
        return false;
    }

    ASSERT(link->contact() != NULL);
    bundle->fwdlog_.update(link, ForwardingInfo::CANCELLED);
    
    return link->clayer()->cancel_bundle(link->contact(), bundle);
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

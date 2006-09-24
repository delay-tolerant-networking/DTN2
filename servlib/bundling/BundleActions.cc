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

    if (link->isopen()) {
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
    size_t total_len = BundleProtocol::formatted_length(bundle);
    
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

    link->stats()->bundles_inflight_++;
    link->stats()->bytes_inflight_ += total_len;
    
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
    bool del =
        BundleDaemon::instance()->try_delete_from_pending(bundle, reason);

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
    bool removed = BundleStore::instance()->del(bundle->bundleid_);
    if (! removed) {
        log_crit("error removing bundle %d from data store!!",
                 bundle->bundleid_);
    }
}

} // namespace dtn

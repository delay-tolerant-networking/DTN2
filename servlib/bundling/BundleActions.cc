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

/**
 * Open a link for bundle transmission. The link should be in
 * state AVAILABLE for this to be called.
 *
 * This may either immediately open the link in which case the
 * link's state will be OPEN, or post a request for the
 * convergence layer to complete the session initiation in which
 * case the link state is OPENING.
 *
 * Note that there is no exposed analog for closing a link since
 * the assumption is that each convergence layer will do the idle
 * connection management.
 */
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

/**
 * Open a link for bundle transmission. The link should be in
 * an open state for this call.
 */
void
BundleActions::close_link(Link* link)
{
    log_debug("closing link %s", link->name());

    if (! link->isopen()) {
        log_err("not closing link %s since already closed", link->name());
        return;
    }

    link->close();
}

/**
 * Create and open a new link for bundle transmissions to the
 * given next hop destination, using the given interface.
 */
void
BundleActions::create_link(std::string& endpoint, Interface* interface)
{
    NOTIMPLEMENTED;
}

/**
 * Initiate transmission of a bundle out the given link.
 *
 * @param bundle	the bundle
 * @param link		the link to send it on
 * @param action	the forwarding action that was taken
 * @param custody_timer	custody timer specification
 */
bool
BundleActions::send_bundle(Bundle* bundle, Link* link,
                           bundle_fwd_action_t action,
                           const CustodyTimerSpec& custody_timer)
{
    log_debug("send bundle *%p to %s link %s (%s)",
              bundle, link->type_str(), link->name(), link->nexthop());

    if (link->state() != Link::OPEN) {
        log_err("send bundle *%p to %s link %s (%s): link not open!!",
              bundle, link->type_str(), link->name(), link->nexthop());
        return false;
    }

    ASSERT(link->contact() != NULL);
    bundle->fwdlog_.add_entry(link, action, ForwardingInfo::IN_FLIGHT,
                              custody_timer);

    link->clayer()->send_bundle(link->contact(), bundle);
    return true;
}

/**
 * Remove a previously queued bundle.
 *
 * @param bundle	the bundle
 * @param nexthop	the next hop consumer
 * @return              true if successful
 */
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

/**
 * Inject a new bundle into the core system, which places it in
 * the pending bundles list as well as in the persistent store.
 * This is typically used by routing algorithms that need to
 * generate their own bundles for distribuing route announcements.
 * It does not, therefore, generate a BundleReceivedEvent.
 *
 * @param bundle		the new bundle
 */
void
BundleActions::inject_bundle(Bundle* bundle)
{
    PANIC("XXX/demmer fix this");
    
    log_debug("inject bundle *%p", bundle);
    BundleDaemon::instance()->pending_bundles()->push_back(bundle);
    store_add(bundle);
}

/**
 * Add the given bundle to the data store.
 */
void
BundleActions::store_add(Bundle* bundle)
{
    log_debug("adding bundle %d to data store", bundle->bundleid_);
    bool added = BundleStore::instance()->add(bundle);
    if (! added) {
        log_crit("error adding bundle %d to data store!!", bundle->bundleid_);
    }
}


/**
 * Update the on-disk version of the given bundle, after it's
 * bookkeeping or header fields have been modified.
 */
void
BundleActions::store_update(Bundle* bundle)
{
    log_debug("updating bundle %d in data store", bundle->bundleid_);
    bool updated = BundleStore::instance()->update(bundle);
    if (! updated) {
        log_crit("error updating bundle %d in data store!!", bundle->bundleid_);
    }
}

/**
 * Remove the given bundle from the data store.
 */
void
BundleActions::store_del(Bundle* bundle)
{
    log_debug("removing bundle %d from data store", bundle->bundleid_);
    bool removed = BundleStore::instance()->del(bundle->bundleid_);
    if (! removed) {
        log_crit("error adding bundle %d to data store!!", bundle->bundleid_);
    }
}

} // namespace dtn

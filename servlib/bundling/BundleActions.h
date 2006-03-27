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
#ifndef _BUNDLE_ACTIONS_H_
#define _BUNDLE_ACTIONS_H_

#include <vector>
#include <oasys/debug/Log.h>
#include "ForwardingInfo.h"

namespace dtn {

class Bundle;
class BundleList;
class CustodyTimerSpec;
class EndpointID;
class Interface;
class Link;
class RouterInfo;

/**
 * Intermediary class that provides the interface that is exposed to
 * routers for the rest of the system. All functions are virtual since
 * the simulator overrides them to provide equivalent functionality
 * (in the simulated environment).
 */
class BundleActions : public oasys::Logger {
public:
    BundleActions() : Logger("BundleActions", "/dtn/bundle/actions") {}
    virtual ~BundleActions() {}
    
    /**
     * Open a link for bundle transmission. The link should be in
     * state AVAILABLE for this to be called.
     *
     * This may either immediately open the link in which case the
     * link's state will be OPEN, or post a request for the
     * convergence layer to complete the session initiation in which
     * case the link state is OPENING.
     */
    virtual void open_link(Link* link);

    /**
     * Open a link for bundle transmission. The link should be in
     * an open state for this call.
     */
    virtual void close_link(Link* link);
    
    /**
     * Create and open a new link for bundle transmissions to the
     * given next hop destination, using the given interface.
     */
    virtual void create_link(std::string& endpoint, Interface* interface);
    
    /**
     * Initiate transmission of a bundle out the given link. The link
     * must already be open.
     *
     * @param bundle		the bundle
     * @param link		the link on which to send the bundle
     * @param action		the forwarding action that was taken
     * @param custody_timer	custody timer specification
     *
     * @return true if the transmission was successfully initiated
     */
    virtual bool send_bundle(Bundle* bundle, Link* link,
                             bundle_fwd_action_t action,
                             const CustodyTimerSpec& custody_timer);
    
    /**
     * Attempt to cancel transmission of a bundle on the given link.
     *
     * @param bundle		the bundle
     * @param link		the link on which the bundle was queued
     *
     * @return			true if successful
     */
    virtual bool cancel_bundle(Bundle* bundle, Link* link);

    /**
     * Inject a new bundle into the core system, which places it in
     * the pending bundles list as well as in the persistent store.
     * This is typically used by routing algorithms that need to
     * generate their own bundles for distribuing route announcements.
     * It does not, therefore, generate a BundleReceivedEvent.
     *
     * @param bundle		the new bundle
     */
    virtual void inject_bundle(Bundle* bundle);

protected:
    // The protected functions (exposed only to the BundleDaemon) and
    // serve solely for the simulator abstraction
    friend class BundleDaemon;
    
    /**
     * Add the given bundle to the data store.
     */
    virtual void store_add(Bundle* bundle);

    /**
     * Update the on-disk version of the given bundle, after it's
     * bookkeeping or header fields have been modified.
     */
    virtual void store_update(Bundle* bundle);

    /**
     * Remove the given bundle from the data store.
     */
    virtual void store_del(Bundle* bundle);
};

} // namespace dtn

#endif /* _BUNDLE_ACTIONS_H_ */

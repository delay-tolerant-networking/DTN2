/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
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
#ifndef _FLOOD_BUNDLE_ROUTER_H_
#define _FLOOD_BUNDLE_ROUTER_H_

#include "TableBasedRouter.h"

namespace dtn {

/**
 * This is the implementation of a flooding based bundle router.
 *
 * The implementation is very simple: The class maintains an internal
 * BundleList in which all bundles are kept until their expiration
 * time. This prevents the main daemon logic from opportunistically
 * deleting bundles when they've been transmitted.
 *
 * Whenever a new link arrives, we add a wildcard route to the table.
 * Then when a bundle arrives, we can stick it on the all_bundles list
 * and just call the base class fwd_to_matching function. The core
 * base class logic then makes sure that a copy of the bundle is
 * forwarded exactly once to each neighbor.
 *
 * XXX/demmer This should be extended to avoid forwarding a bundle
 * back to the node from which it arrived. With the upcoming
 * bidirectional link changes, this should be able to be done easily.
 */
class FloodBundleRouter : public TableBasedRouter {
public:
    /**
     * Constructor.
     */
    FloodBundleRouter();
    
    /**
     * Event handler for new bundle arrivals.
     *
     * Queue the bundle on the pending delivery list, and then
     * searches through the route table to find any matching next
     * contacts, filling in the action list with forwarding decisions.
     */
    void handle_bundle_received(BundleReceivedEvent* event);
    
    /**
     * When a link is created, add a new route for it.
     */
    void handle_link_created(LinkCreatedEvent* event);
    
    /**
     * Default event handler when bundles expire
     */
    void handle_bundle_expired(BundleExpiredEvent* event);
    
protected:
    /// To ensure bundles aren't deleted by the system just after they
    /// are forwarded, we hold them all in this separate list.
    BundleList all_bundles_;

    /// Wildcard pattern to match all bundles.
    EndpointIDPattern all_eids_;
};

} // namespace dtn

#endif /* _FLOOD_BUNDLE_ROUTER_H_ */

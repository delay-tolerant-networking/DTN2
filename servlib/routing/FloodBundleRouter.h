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
 * This is the implementation of the basic bundle routing algorithm
 * that only does flood routing. Routes can be parsed from a
 * configuration file or injected via the command line and/or
 * management interfaces.
 *
 * As a result, the class simply uses the default base class handlers
 * for all bundle events that flow through it, and the only code here
 * is that which parses the flood route configuration file.
 */
 
class FloodBundleRouter : public TableBasedRouter {
public:
    FloodBundleRouter();
    ~FloodBundleRouter();
    
    const int id() { return id_;}
protected:
    int id_;
    /**
     * Default event handler for new bundle arrivals.
     *
     * Queue the bundle on the pending delivery list, and then
     * searches through the route table to find any matching next
     * contacts, filling in the action list with forwarding decisions.
     */
    void handle_bundle_received(BundleReceivedEvent* event);
    
    /**
     * Default event handler when bundles are transmitted.
     *
     * If the bundle was only partially transmitted, this calls into
     * the fragmentation module to create a new bundle fragment and
     * enqeues the new fragment on the appropriate list.
     */
    void handle_bundle_transmitted(BundleTransmittedEvent* event);

    /**
     * Default event handler when a link is created.
     */
    void handle_link_created(LinkCreatedEvent* event);
    
    /**
     * Default event handler when a contact is down
     */
    void handle_contact_down(ContactDownEvent* event);
    
    /**
     * Called whenever a new next hop link arrives. This walks the
     * list of all pending bundles, forwarding all matches to the new
     * link.
     */
    void new_next_hop(const BundleTuplePattern& dest, Link* next_hop);


    int fwd_to_matching(Bundle *bundle, bool include_local);

protected:
    BundleTuplePattern all_tuples_;
};

} // namespace dtn

#endif /* _FLOOD_BUNDLE_ROUTER_H_ */

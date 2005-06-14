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
#include "BundleMapping.h"

namespace dtn {

class Bundle;
class BundleList;
class BundleConsumer;
class Link;
class RouterInfo;

/**
 * Intermediary class that provides the interface from the router to
 * the rest of the system. All functions are virtual since the
 * simulator overrides them to provide equivalent functionality (in
 * the simulated environment).
 */
class BundleActions : public oasys::Logger {
public:
    BundleActions() : Logger("/bundle/actions") {}
    virtual ~BundleActions() {}
    
    /**
     * Queue a bundle for delivery on the given next hop. In queueing
     * the bundle for delivery, this creates a new BundleMapping to
     * store any router state about this decision.
     *
     * @param bundle		the bundle
     * @param nexthop		the next hop consumer
     * @param fwdaction		action type code to be returned in the
     *                          BUNDLE_TRANSMITTED event
     * @param mapping_grp	group identifier for the set of mappings
     * @param expiration	expiration time for this mapping
     * @param router_info	opaque slot for associated routing info
     */
    virtual void enqueue_bundle(Bundle* bundle, BundleConsumer* nexthop,
                                bundle_fwd_action_t fwdaction,
                                int mapping_grp = 0, u_int32_t expiration = 0,
                                RouterInfo* router_info = 0);

    /**
     * Remove a previously queued bundle.
     *
     * @param bundle		the bundle
     * @param nexthop		the next hop consumer
     * @return                  true if successful
     */
    virtual bool dequeue_bundle(Bundle* bundle, BundleConsumer* nexthop);

    /**
     * Move the all queued bundles from one consumer to another.
     */
    virtual void move_contents(BundleConsumer* source, BundleConsumer* dest);

    /**
     * Add the given bundle to the data store.
     */
    virtual void store_add(Bundle* bundle);

    /**
     * Remove the given bundle from the data store.
     */
    virtual void store_del(Bundle* bundle);

    /**
     * Open a link for bundle transmission.
     */
    virtual void open_link(Link* link);

    /**
     * Close the given link.
     */
    virtual void close_link(Link* link);
};

} // namespace dtn

#endif /* _BUNDLE_ACTIONS_H_ */

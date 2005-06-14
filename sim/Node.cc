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
#include "SimEvent.h"
#include "Node.h"
#include "SimBundleActions.h"
#include "bundling/ContactManager.h"
#include "bundling/FragmentManager.h"
#include "bundling/Link.h"
#include "bundling/BundleDaemon.h"
#include "routing/BundleRouter.h"
#include "routing/RouteTable.h"
#include "reg/Registration.h"

using namespace dtn;

namespace dtnsim {

Node::Node(const char* name)
    : BundleDaemon(), name_(name),
      next_bundleid_(0), next_regid_(Registration::MAX_RESERVED_REGID)
{
    logpathf("/node/%s", name);
    log_info("node %s initializing...", name);
}

/**
 * Virtual initialization function.
 */
void
Node::do_init()
{
    actions_ = new SimBundleActions();
    eventq_ = new std::queue<BundleEvent*>();

    BundleDaemon::instance_ = this;
    router_ = BundleRouter::create_router(BundleRouter::Config.type_.c_str());
}

/**
 * Virtual post function, overridden in the simulator to use the
 * modified event queue.
 */
void
Node::post_event(BundleEvent* event)
{
    eventq_->push(event);
}

/**
 * Process all pending bundle events until the queue is empty.
 */
void
Node::process_bundle_events()
{
    log_debug("processing all bundle events");
    BundleEvent* event;
    while (!eventq_->empty()) {
        event = eventq_->front();
        eventq_->pop();
        handle_event(event);
        router_->handle_event(event);
        delete event;
    }
}


void
Node::process(SimEvent* simevent)
{
    set_active();
    
    log_debug("handling event %s", simevent->type_str());

    switch(simevent->type()) {
    case SIM_ROUTER_EVENT:
        post_event(((SimRouterEvent*)simevent)->event_);
        break;

    case SIM_ADD_LINK: {
        SimAddLinkEvent* e = (SimAddLinkEvent*)simevent;

        // Add the link to contact manager, which posts a
        // LinkCreatedEvent to the daemon
        contactmgr_->add_link(e->link_);
        
        break;
    }
        
    case SIM_ADD_ROUTE: {
        SimAddRouteEvent* e = (SimAddRouteEvent*)simevent;
        
        BundleConsumer* nexthop = NULL;
        nexthop = contactmgr()->find_link(e->nexthop_.c_str());

        // XXX/demmer handle search by endpoint

        if (nexthop == NULL) {
            PANIC("no such link or node exists %s", e->nexthop_.c_str());
        }

        // XXX/demmer fix this FORWARD_COPY
        RouteEntry* entry = new RouteEntry(e->dest_, nexthop, FORWARD_COPY);
        post_event(new RouteAddEvent(entry));
        break;
    }
            
    default:
        PANIC("no Node handler for event %s", simevent->type_str());
    }
    
    process_bundle_events();

    delete simevent;
}

} // namespace dtnsim

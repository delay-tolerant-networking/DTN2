/*
 *    Copyright 2004-2006 Intel Corporation
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

#include "SimEvent.h"
#include "Node.h"
#include "SimBundleActions.h"
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"
#include "contacts/Link.h"
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
    router_ = BundleRouter::create_router(BundleRouter::config_.type_.c_str());
}

/**
 * Virtual post function, overridden in the simulator to use the
 * modified event queue.
 */
void
Node::post_event(BundleEvent* event, bool at_back)
{
    (void)at_back;
    log_debug("posting event (%p) with type %s at %s ",event, event->type_str(),at_back ? "back" : "head");
	
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
        delete event;
        log_debug("event (%p) %s processed and deleted",event,event->type_str());
    }
    log_debug("done processing all bundle events");
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
        
        Link* link = contactmgr()->find_link(e->nexthop_.c_str());

        // XXX/demmer handle search by endpoint

        if (link == NULL) {
            PANIC("no such link or node exists %s", e->nexthop_.c_str());
        }

        // XXX/demmer fix this ForwardingInfo::COPY_ACTION
        RouteEntry* entry = new RouteEntry(e->dest_, link);
        entry->action_ = ForwardingInfo::COPY_ACTION;
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

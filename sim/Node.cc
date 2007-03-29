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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <oasys/thread/Timer.h>
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

//----------------------------------------------------------------------
Node::Node(const char* name)
    : BundleDaemon(), name_(name)
{
    logpathf("/node/%s", name);
    log_info("node %s initializing...", name);

    BundleDaemon::is_simulator_ = true;
}

//----------------------------------------------------------------------
void
Node::do_init()
{
    BundleDaemon::instance_ = this;
    
    actions_ = new SimBundleActions();
    eventq_ = new std::queue<BundleEvent*>();

    // forcibly create a new timer system
    oasys::Singleton<oasys::TimerSystem>::force_set_instance(NULL);
    oasys::TimerSystem::create();
    timersys_ = oasys::TimerSystem::instance();
}

//----------------------------------------------------------------------
void
Node::configure()
{
    BundleDaemon::instance_ = this;
    router_ = BundleRouter::create_router(BundleRouter::config_.type_.c_str());
    router_->initialize();
}

//----------------------------------------------------------------------
void
Node::post_event(BundleEvent* event, bool at_back)
{
    (void)at_back;
    log_debug("posting event (%p) with type %s at %s ",
              event, event->type_str(),at_back ? "back" : "head");
        
    eventq_->push(event);
}

//----------------------------------------------------------------------
bool
Node::process_one_bundle_event()
{
    BundleEvent* event;
    if (!eventq_->empty()) {
        event = eventq_->front();
        eventq_->pop();
        handle_event(event);
        delete event;
        log_debug("event (%p) %s processed and deleted",event,event->type_str());
        return true;
    }
    return false;
}

//----------------------------------------------------------------------
bool
Node::process_bundle_events()
{
    log_debug("processing all bundle events");
    bool processed_event = false;
    while (process_one_bundle_event()) {
        processed_event = true;
    }
    return processed_event;
}

//----------------------------------------------------------------------
void
Node::process(SimEvent* e)
{
    switch (e->type()) {
    case SIM_BUNDLE_EVENT:
        post_event(((SimBundleEvent*)e)->event_);
        break;
        
    default:
        NOTREACHED;
    }
}

} // namespace dtnsim

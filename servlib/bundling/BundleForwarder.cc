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

#include "Bundle.h"
#include "BundleEvent.h"
#include "BundleAction.h"
#include "BundleList.h"
#include "BundleForwarder.h"
#include "Contact.h"
#include "routing/BundleRouter.h"
#include "storage/BundleStore.h"

namespace dtn {

BundleForwarder* BundleForwarder::instance_ = NULL;

BundleForwarder::BundleForwarder()
    : Logger("/bundle/fwd")
{
    bundles_received_ = 0;
    bundles_sent_local_ = 0;
    bundles_sent_remote_ = 0;
    bundles_expired_ = 0;
}

/**
 * Queues the given event on the pending list of events.
 */
void
BundleForwarder::post(BundleEvent* event)
{
    __log_debug("/bundle/fwd", "posting event with type %s", event->type_str());

    switch(event->type_) {

    case BUNDLE_RECEIVED:
        instance_->bundles_received_++;
        break;

    case BUNDLE_TRANSMITTED:
        if (((BundleTransmittedEvent*)event)->consumer_->is_local()) {
            instance_->bundles_sent_local_++;
        } else {
            instance_->bundles_sent_remote_++;
        }
        break;

    case BUNDLE_EXPIRED:
        instance_->bundles_expired_++;
        break;
        
    default:
        break;
    }
    
    instance_->eventq_.push(event);
}


/**
 * Format the given StringBuffer with the current statistics value.
 */
void
BundleForwarder::get_statistics(oasys::StringBuffer* buf)
{
    buf->appendf("%d pending -- %d received -- %d sent_local -- %d sent_remote "
                 "-- %d expired",
                 active_router()->pending_bundles()->size(),
                 bundles_received_,
                 bundles_sent_local_,
                 bundles_sent_remote_,
                 bundles_expired_);
}

/**
 * Return a pointer to the pending bundle list.
 */
BundleList*
BundleForwarder::pending_bundles()
{
    return active_router()->pending_bundles();
}

/**
 * Routine that actually effects the forwarding operations as
 * returned from the BundleRouter.
 */
void
BundleForwarder::process(BundleAction* action)
{
    Bundle* bundle;
    bundle = action->bundleref_.bundle();

    log_debug("executing action %s",action->type_str());
    
    switch (action->action_) {
    case ENQUEUE_BUNDLE:
    {
        BundleEnqueueAction* enqaction = (BundleEnqueueAction*) action;
        BundleConsumer* nexthop = enqaction->nexthop_;
        
        log_debug("forward bundle %d on consumer type %s",
                  bundle->bundleid_, nexthop->type_str());
        

        if (nexthop->is_queued(bundle)) {
            log_debug("not forwarding to %s since already queued",
                      nexthop->dest_tuple()->c_str());
        } else {
            nexthop->enqueue_bundle(bundle, &enqaction->mapping_);
        }
        break;
    }
    case STORE_ADD: {
        bool added = BundleStore::instance()->add(bundle);
        ASSERT(added);
        break;
    }
    case STORE_DEL: {
        bool removed = BundleStore::instance()->del(bundle->bundleid_);
        ASSERT(removed);
        break;
    }
    case OPEN_LINK: {
        OpenLinkAction* open_link_action = (OpenLinkAction*) action;
        Link* link = open_link_action->link_;
        if (!link->isopen())
            link->open();
        break;
    }
    default:
        PANIC("unimplemented action code %s",
              bundle_action_toa(action->action_));
    }
    
    delete action;
}

/**
 * The main run loop.
 */
void
BundleForwarder::run()
{
    BundleEvent* event;
    BundleActionList actions;
    BundleActionList::iterator iter;
    
    while (1) {
        // grab an event off the queue, blocking until we get one
        event = eventq_.pop_blocking();
        ASSERT(event);

        // always clear the action list
        actions.clear();
        
        // dispatch to the router to fill in the actions list
        router_->handle_event(event, &actions);
        
        // process the actions
        for (iter = actions.begin(); iter != actions.end(); ++iter) {
            process(*iter);
        }
    }
}


} // namespace dtn

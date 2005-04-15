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
#include "BundleActions.h"
#include "BundleConsumer.h"
#include "BundleEvent.h"
#include "BundleDaemon.h"
#include "ContactManager.h"
#include "FragmentManager.h"
#include "routing/BundleRouter.h"

namespace dtn {

BundleDaemon* BundleDaemon::instance_ = NULL;

BundleDaemon::BundleDaemon()
    : Logger("/bundle/daemon")
{
    bundles_received_ = 0;
    bundles_sent_local_ = 0;
    bundles_sent_remote_ = 0;
    bundles_expired_ = 0;

    pending_bundles_ = new BundleList("pending_bundles");
    custody_bundles_ = new BundleList("custody_bundles");

    contactmgr_ = new ContactManager();
    fragmentmgr_ = new FragmentManager();

    router_ = 0;
}

/**
 * Virtual initialization function. Overridden in the simulator by
 * the Node class. 
 */
void
BundleDaemon::do_init()
{
    actions_ = new BundleActions();
    eventq_ = new oasys::MsgQueue<BundleEvent*>;
}

/**
 * Dispatches to the virtual post_event implementation.
 */
void
BundleDaemon::post(BundleEvent* event)
{
    instance_->post_event(event);
}

/**
 * Virtual post function, overridden in the simulator to use the
 * modified event queue.
 */
void
BundleDaemon::post_event(BundleEvent* event)
{
    log_debug("posting event with type %s", event->type_str());
    eventq_->push(event);
}

/**
 * Format the given StringBuffer with the current statistics value.
 */
void
BundleDaemon::get_statistics(oasys::StringBuffer* buf)
{
    buf->appendf("%d pending -- %d received -- %d sent_local -- %d sent_remote "
                 "-- %d expired",
                 pending_bundles()->size(),
                 bundles_received_,
                 bundles_sent_local_,
                 bundles_sent_remote_,
                 bundles_expired_);
}

/**
 * Update statistics based on the event arrival.
 */
void
BundleDaemon::update_statistics(BundleEvent* event)
{
    switch(event->type_) {

    case BUNDLE_RECEIVED:
        bundles_received_++;
        break;

    case BUNDLE_TRANSMITTED:
        if (((BundleTransmittedEvent*)event)->consumer_->is_local()) {
            bundles_sent_local_++;
        } else {
            bundles_sent_remote_++;
        }
        break;

    case BUNDLE_EXPIRED:
        bundles_expired_++;
        break;
        
    default:
        break;
    }
}
    

/**
 * The main run loop.
 */
void
BundleDaemon::run()
{
    BundleEvent* event;
    
    while (1) {
        // grab an event off the queue, blocking until we get one
        event = eventq_->pop_blocking();
        ASSERT(event);

        // update any stats
        update_statistics(event);

        // dispatch to the router, passing along any actions
        router_->handle_event(event);
    }
}


} // namespace dtn

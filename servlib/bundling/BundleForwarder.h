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
#ifndef _BUNDLE_FORWARDING_H_
#define _BUNDLE_FORWARDING_H_

#include <vector>

#include <oasys/debug/Log.h>
#include <oasys/thread/Thread.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/util/StringBuffer.h>

namespace dtn {

class Bundle;
class BundleAction;
class BundleActionList;
class BundleConsumer;
class BundleEvent;
class BundleList;
class BundleRouter;

/**
 * Class that handles the flow of bundle events through the system.
 */
class BundleForwarder : public oasys::Logger, public oasys::Thread {
public:
    /**
     * Singleton accessor.
     */
    static BundleForwarder* instance() {
        ASSERT(instance_ != NULL);
        return instance_;
    }

    /**
     * Constructor.
     */
    BundleForwarder();

    /**
     * Boot time initializer.
     */
    static void init(BundleForwarder* instance)
    {
        if (instance_ != NULL) {
            PANIC("BundleForwarder already initialized");
        }
        instance_ = instance;
    }
    
    /**
     * Queues the given event on the pending list of events.
     */
    static void post(BundleEvent* event);

    /**
     * Returns the currently active bundle router.
     */
    BundleRouter* active_router() { return router_; }

    /**
     * Sets the active router.
     */
    void set_active_router(BundleRouter* router) { router_ = router; }

    /**
     * Format the given StringBuffer with the current statistics value.
     */
    void get_statistics(oasys::StringBuffer* buf);

    /**
     * Return a pointer to the pending bundle list.
     */
    BundleList* pending_bundles();

protected:
    /**
     * Routine that implements a particular action, as returned from
     * the BundleRouter.
     */
    void process(BundleAction* action);

    /**
     * Main thread function that dispatches events.
     */
    void run();
    
    /// The active bundle router
    BundleRouter* router_;

    /// The event queue
    oasys::MsgQueue<BundleEvent*> eventq_;

    /// Statistics
    u_int32_t bundles_received_;
    u_int32_t bundles_sent_local_;
    u_int32_t bundles_sent_remote_;
    u_int32_t bundles_expired_;
    
    static BundleForwarder* instance_;
};

} // namespace dtn

#endif /* _BUNDLE_FORWARDING_H_ */

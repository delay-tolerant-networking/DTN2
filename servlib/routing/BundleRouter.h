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
#ifndef _BUNDLE_ROUTER_H_
#define _BUNDLE_ROUTER_H_

#include <vector>
#include <oasys/debug/Logger.h>
#include <oasys/thread/Thread.h>
#include <oasys/util/StringUtils.h>

#include "bundling/BundleEvent.h"
#include "bundling/BundleEventHandler.h"
#include "bundling/BundleTuple.h"

namespace dtn {

class BundleActions;
class BundleConsumer;
class BundleRouter;
class StringBuffer;

/**
 * Typedef for a list of bundle routers.
 */
typedef std::vector<BundleRouter*> BundleRouterList;

/**
 * The BundleRouter is the main decision maker for all routing
 * decisions related to bundles.
 *
 * It receives events from the BundleDaemon having been posted by
 * other components. These events include all operations and
 * occurrences that may affect bundle delivery, including new bundle
 * arrival, contact arrival, timeouts, etc.
 *
 * In response to each event the router may call one of the action
 * functions implemented by the BundleDaemon. Note that to support the
 * simulator environment, all interactions with the rest of the system
 * should go through the singleton BundleAction classs.
 *
 * To support prototyping of different routing protocols and
 * frameworks, the base class has a list of prospective BundleRouter
 * implementations, and at boot time, one of these is selected as the
 * active routing algorithm. As new algorithms are added to the
 * system, new cases should be added to the "create_router" function.
 */
class BundleRouter : public BundleEventHandler {
public:
    /**
     * Factory method to create the correct subclass of BundleRouter
     * for the registered algorithm type.
     */
    static BundleRouter* create_router(const char* type);

    /**
     * Config variables. These must be static since they're set by the
     * config parser before the router object is created. At
     * initialization time, the local_tuple variables are propagated
     * into the actual router instance.
     */
    static struct config_t {
        std::string         type_;
        BundleTuple         local_tuple_;
    } Config;
    
    /**
     * Destructor
     */
    virtual ~BundleRouter();

    /*
     *  called after all the global data structures are set up
     */
    virtual void initialize();

    /**
     * Pure virtual event handler function (copied from
     * BundleEventHandler for clarity).
     */
    virtual void handle_event(BundleEvent* event) = 0;
    
    /**
     * Format the given StringBuffer with current routing info.
     */
    virtual void get_routing_state(oasys::StringBuffer* buf) = 0;
    
    /**
     * Accessor for the local tuple.
     */
    const BundleTuple& local_tuple() { return local_tuple_; }

    /**
     * Assignment function for the local tuple string.
     */
    void set_local_tuple(const char* tuple_str) {
        local_tuple_.assign(tuple_str);
    }

protected:
    /**
     * Constructor
     */
    BundleRouter(const std::string& name);

    /// Name of this particular router
    std::string name_;
    
    /**
     * The default tuple for reaching this router, used for bundle
     * status reports, etc. Note that the region must be one of the
     * locally configured regions.
     */
    BundleTuple local_tuple_;
    
    /// The list of all bundles still pending delivery
    BundleList* pending_bundles_;

    /// The list of all bundles that I have custody of
    BundleList* custody_bundles_;

    /// The actions interface, set by the BundleDaemon when the router
    /// is initialized.
    BundleActions* actions_;
    
};

} // namespace dtn

#endif /* _BUNDLE_ROUTER_H_ */

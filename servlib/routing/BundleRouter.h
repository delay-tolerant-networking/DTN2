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

#ifndef _BUNDLE_ROUTER_H_
#define _BUNDLE_ROUTER_H_

#include <vector>
#include <oasys/debug/Logger.h>
#include <oasys/thread/Thread.h>
#include <oasys/util/StringUtils.h>

#include "bundling/BundleEvent.h"
#include "bundling/BundleEventHandler.h"
#include "naming/EndpointID.h"

namespace dtn {

class BundleActions;
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
     * config parser before any router objects are created.
     */
    static struct Config {
        Config();
        
        /// The routing algorithm type
        std::string type_;
        
        /// Whether or not to add routes for nexthop links that know
        /// the remote endpoint id (default true)
        bool add_nexthop_routes_;
        
        /// Default priority for new routes
        int default_priority_;
                
    } config_;
    
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
     * for registration with the BundleDaemon
     */
    virtual void shutdown();
    
protected:
    /**
     * Constructor
     */
    BundleRouter(const char* classname, const std::string& name);

    /// Name of this particular router
    std::string name_;
    
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

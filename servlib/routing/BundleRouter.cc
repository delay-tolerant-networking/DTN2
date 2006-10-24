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


#include <stdlib.h>

#include "BundleRouter.h"
#include "bundling/BundleDaemon.h"

#include "StaticBundleRouter.h"
#include "FloodBundleRouter.h"
#include "NeighborhoodRouter.h"
#include "ProphetRouter.h"
#include "LinkStateRouter.h"
#include "TcaRouter.h"

namespace dtn {

/**
 * Config defaults are set in RouteCommand.cc
 */
BundleRouter::config_t BundleRouter::Config;

/**
 * Factory method to create the correct subclass of BundleRouter
 * for the registered algorithm type.
 */
BundleRouter*
BundleRouter::create_router(const char* type)
{
    if (strcmp(type, "static") == 0) {
        return new StaticBundleRouter();
    }
    else if (strcmp(type, "neighborhood") == 0) {
        return new NeighborhoodRouter();
    }
    else if (strcmp(type, "prophet") == 0) {
        return new ProphetRouter();
    }
    else if (strcmp(type, "flood") == 0) {
        return new FloodBundleRouter();
    }
    else if (strcmp(type, "linkstate") == 0) {
        return new LinkStateRouter();
    }    
    else if (!strcmp(type, "tca_router")) {
        return new TcaRouter(TcaRouter::TCA_ROUTER);
    }
    else if (!strcmp(type, "tca_gateway")) {
        return new TcaRouter(TcaRouter::TCA_GATEWAY);
    }
    else {
        PANIC("unknown type %s for router", type);
    }
}

/**
 * Constructor.
 */
BundleRouter::BundleRouter(const char* classname, const std::string& name)
    : BundleEventHandler(classname, "/dtn/route"),
      name_(name)
{
    logpathf("/dtn/route/%s", name.c_str());
    
    actions_ = BundleDaemon::instance()->actions();

    // XXX/demmer maybe change this?
    pending_bundles_ = BundleDaemon::instance()->pending_bundles();
    custody_bundles_ = BundleDaemon::instance()->custody_bundles();
}

/* 
 * Virtual initialization function that's called after all the main
 * components are set up.
 */
void
BundleRouter::initialize()
{
}

/**
 * Destructor
 */
BundleRouter::~BundleRouter()
{
}

} // namespace dtn

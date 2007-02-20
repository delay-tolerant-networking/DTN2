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

#include <config.h>
#include "BundleRouter.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleList.h"
#include "storage/BundleStore.h"

#include "StaticBundleRouter.h"
#include "FloodBundleRouter.h"
#include "NeighborhoodRouter.h"
#include "ProphetRouter.h"
#include "LinkStateRouter.h"
#include "ShortestPathRouter.h"
#include "ExternalRouter.h"
#include "TcaRouter.h"

namespace dtn {

//----------------------------------------------------------------------
BundleRouter::Config::Config()
    : type_("static"),
      add_nexthop_routes_(true),
      default_priority_(0),
      storage_quota_(0) {}

BundleRouter::Config BundleRouter::config_;

//----------------------------------------------------------------------
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
    else if (strcmp(type, "shortest_path") == 0) {
        return new ShortestPathRouter();
    }    
    else if (!strcmp(type, "tca_router")) {
        return new TcaRouter(TcaRouter::TCA_ROUTER);
    }
    else if (!strcmp(type, "tca_gateway")) {
        return new TcaRouter(TcaRouter::TCA_GATEWAY);
    }
#ifdef XERCES_C_ENABLED
    else if (strcmp(type, "external") == 0) {
        return new ExternalRouter();
    }    
#endif
    else {
        PANIC("unknown type %s for router", type);
    }
}

//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
void
BundleRouter::initialize()
{
}

//----------------------------------------------------------------------
BundleRouter::~BundleRouter()
{
}

//----------------------------------------------------------------------
bool
BundleRouter::accept_bundle(Bundle* bundle, int* errp)
{
    // XXX/demmer this could (should?) be abstracted into a
    // StoragePolicy class hierarchy of some sort

    BundleStore* bs = BundleStore::instance();
    if (bs->payload_quota() != 0 &&
        (bs->total_size() + bundle->payload_.length() > bs->payload_quota()))
    {
        log_info("accept_bundle: rejecting bundle *%p since "
                 "cur size %llu + bundle size %zu > quota %llu",
                 bundle, U64FMT(bs->total_size()), bundle->payload_.length(),
                 U64FMT(bs->payload_quota()));
        *errp = BundleProtocol::REASON_DEPLETED_STORAGE;
        return false;
    } 

    *errp = 0;
    return true;
}

//----------------------------------------------------------------------
void
BundleRouter::shutdown()
{
}

} // namespace dtn

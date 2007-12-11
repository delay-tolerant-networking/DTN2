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
#  include <dtn-config.h>
#endif

#include <stdlib.h>

#include "BundleRouter.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleList.h"
#include "storage/BundleStore.h"

#include "StaticBundleRouter.h"
#include "FloodBundleRouter.h"
#include "ProphetRouter.h"
#include "DTLSRRouter.h"
#include "ExternalRouter.h"
#include "TcaRouter.h"

namespace dtn {

//----------------------------------------------------------------------
BundleRouter::Config::Config()
    : type_("static"),
      add_nexthop_routes_(true),
      open_discovered_links_(true),
      default_priority_(0),
      max_route_to_chain_(10),
      storage_quota_(0) {}

BundleRouter::Config BundleRouter::config_;

//----------------------------------------------------------------------
BundleRouter*
BundleRouter::create_router(const char* type)
{
    if (strcmp(type, "static") == 0) {
        return new StaticBundleRouter();
    }
    else if (strcmp(type, "prophet") == 0) {
        return new ProphetRouter();
    }
    else if (strcmp(type, "flood") == 0) {
        return new FloodBundleRouter();
    }
    else if (strcmp(type, "dtlsr") == 0) {
        return new DTLSRRouter();
    }    
    else if (!strcmp(type, "tca_router")) {
        return new TcaRouter(TcaRouter::TCA_ROUTER);
    }
    else if (!strcmp(type, "tca_gateway")) {
        return new TcaRouter(TcaRouter::TCA_GATEWAY);
    }
#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)
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
bool
BundleRouter::should_fwd(const Bundle* bundle, const LinkRef& link,
                         ForwardingInfo::action_t action)
{
    ForwardingInfo info;
    bool found = bundle->fwdlog_.get_latest_entry(link, &info);

    if (found) {
        ASSERT(info.state() != ForwardingInfo::NONE);
    } else {
        ASSERT(info.state() == ForwardingInfo::NONE);
    }

    // check if we've already sent or are in the process of sending
    // the bundle on this link
    if (info.state() == ForwardingInfo::TRANSMITTED ||
        info.state() == ForwardingInfo::IN_FLIGHT)
    {
        log_debug("should_fwd bundle %d: "
                  "skip %s due to forwarding log entry %s",
                  bundle->bundleid_, link->name(),
                  ForwardingInfo::state_to_str(info.state()));
        return false;
    }

    // check if we're trying to send it right back where it came from
    //
    // XXX/demmer this is bad -- some routing schemes might want to do
    // this
    if (link->remote_eid() == bundle->prevhop_ &&
        link->remote_eid() != EndpointID::NULL_EID())
    {
        log_debug("should_fwd bundle %d: "
                  "skip %s since remote eid %s == bundle prevhop",
                  bundle->bundleid_, link->name(),
                  link->remote_eid().c_str());
        return false;
    }

    // check if we've already sent the bundle to the node via some
    // other link
    size_t count = bundle->fwdlog_.get_count(
        link->remote_eid(),
        ForwardingInfo::TRANSMITTED | ForwardingInfo::IN_FLIGHT);

    if (count > 0)
    {
        log_debug("should_fwd bundle %d: "
                  "skip %s since already sent %zu times to remote eid %s",
                  bundle->bundleid_, link->name(),
                  count, link->remote_eid().c_str());
        return false;
    }

    // if the bundle has a a singleton destination endpoint, then
    // check if we already forwarded it or are planning to forward it
    // somewhere else. if so, we shouldn't forward it again
    if (bundle->singleton_dest_ && action == ForwardingInfo::FORWARD_ACTION)
    {
        size_t count = bundle->fwdlog_.get_count(
            ForwardingInfo::TRANSMITTED |
            ForwardingInfo::IN_FLIGHT,
            action);

        if (count > 0) {
            log_debug("should_fwd bundle %d: "
                      "skip %s since already transmitted (count %zu)",
                      bundle->bundleid_, link->name(), count);
            return false;
        } else {
            log_debug("should_fwd bundle %d: "
                      "link %s ok since transmission count=%zu",
                      bundle->bundleid_, link->name(), count);
        }
    }

    // otherwise log the reason why we should send it
    log_debug("should_fwd bundle %d: "
              "match %s: forwarding log entry %s",
              bundle->bundleid_, link->name(),
              ForwardingInfo::state_to_str(info.state()));

    return true;
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
    // XXX/demmer this decision should be abstracted into a
    // StoragePolicy class of some sort. for now just use a
    // statically-configured payload limit
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
BundleRouter::tcl_dump_state(oasys::StringBuffer* buf)
{
    buf->append("{}");
}

//----------------------------------------------------------------------
void
BundleRouter::recompute_routes()
{
}

//----------------------------------------------------------------------
void
BundleRouter::shutdown()
{
}

} // namespace dtn

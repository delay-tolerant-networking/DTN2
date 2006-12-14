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


#include <oasys/util/StringBuffer.h>

#include "Simulator.h"
#include "SimRegistration.h"
#include "Topology.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleEvent.h"
#include "storage/GlobalStore.h"

using namespace dtn;

namespace dtnsim {

SimRegistration::SimRegistration(Node* node, const EndpointID& endpoint)
    : Registration(GlobalStore::instance()->next_regid(),
                   endpoint, DEFER, 0), node_(node)
{
    logpathf("/reg/%s/%d", node->name(), regid_);

    log_debug("new sim registration");
}

void
SimRegistration::deliver_bundle(Bundle* bundle)
{
    size_t payload_len = bundle->payload_.length();

    log_info("N[%s]: RCV id:%d %s -> %s size:%zu",
             node_->name(), bundle->bundleid_,
             bundle->source_.c_str(), bundle->dest_.c_str(),
             payload_len);

    BundleDaemon::post(new BundleDeliveredEvent(bundle, this));

    // hold a reference on the bundle to put it in the arrivals table
    double now = Simulator::time();
    arrivals_[now] = bundle; 
}

} // namespace dtnsim

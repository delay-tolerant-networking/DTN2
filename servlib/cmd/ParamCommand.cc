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


#include "ParamCommand.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundlePayload.h"
#include "bundling/CustodyTimer.h"
#include "conv_layers/TCPConvergenceLayer.h"

namespace dtn {

ParamCommand::ParamCommand() 
    : TclCommand("param")
{
    bind_i("payload_mem_threshold",
           (int*)&BundlePayload::mem_threshold_, 16384,
           "The bundle size below which bundles are held in memory. "
           "(Default is 16k.)");

    bind_b("payload_test_no_remove",
           &BundlePayload::test_no_remove_, false,
           "Boolean to control not removing bundles (for testing).");

    bind_i("proactive_frag_threshold",
           (int*)&BundleDaemon::params_.proactive_frag_threshold_, -1,
           "Proactively fragment bundles to this size. (0 is off.)");

    bind_b("early_deletion",
           &BundleDaemon::params_.early_deletion_, true,
           "Delete forwarded / delivered bundles before they've expired "
           "(default is true)");

    bind_b("accept_custody",
           &BundleDaemon::params_.accept_custody_, true,
           "Accept custody when requested (default is true)");

    bind_b("reactive_frag_enabled",
           &BundleDaemon::params_.reactive_frag_enabled_, true,
           "Is reactive fragmentation enabled (default is true)");

    bind_b("retry_reliable_unacked",
           &BundleDaemon::params_.retry_reliable_unacked_, true,
           "Retry unacked transmissions on reliable CLs (default is true)");

    bind_b("test_permuted_delivery",
           &BundleDaemon::params_.test_permuted_delivery_, false,
           "Permute the order of bundles before delivering to registrations");

    bind_i("link_min_retry_interval",
           &Link::default_params_.min_retry_interval_, 5,
           "Default minimum connection retry interval for links");

    bind_i("link_max_retry_interval",
           &Link::default_params_.max_retry_interval_, 10 * 60,
           "Default maximum connection retry interval for links");

    // defaults for these are set in the CustodyTimerSpec defaults
    // static initializer
    bind_i("custody_timer_min",
           &CustodyTimerSpec::defaults_.min_,
           "default value for custody timer min");
    
    bind_i("custody_timer_lifetime_pct",
           &CustodyTimerSpec::defaults_.lifetime_pct_,
           "default value for custody timer lifetime percentage");
    
    bind_i("custody_timer_max",
           &CustodyTimerSpec::defaults_.max_,
           "default value for custody timer max");
}
    
} // namespace dtn

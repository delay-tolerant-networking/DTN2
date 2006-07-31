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

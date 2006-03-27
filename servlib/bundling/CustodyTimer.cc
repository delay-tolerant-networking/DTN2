/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 *  2006 Intel Corporation. All rights reserved. 
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

#include <algorithm>
#include <oasys/util/OptParser.h>

#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleEvent.h"
#include "CustodyTimer.h"

namespace dtn {

/**
 * Default custody timer specification:
 *
 * base: 30 minutes
 * lifetime percent: 25%
 * limit: unlimited
 */
CustodyTimerSpec CustodyTimerSpec::defaults_(30 * 60, 25, 0);

//----------------------------------------------------------------------
u_int32_t
CustodyTimerSpec::calculate_timeout(const Bundle* bundle) const
{
    u_int32_t timeout = base_;
    timeout += (u_int32_t)((double)lifetime_pct_ * bundle->expiration_ / 100.0);

    if (limit_ != 0) {
        timeout = std::min(timeout, limit_);
    }
    
    log_debug("/dtn/bundle/custody_timer", "calculate_timeout: "
              "base %u, lifetime_pct %u, expiration %u, limit %u: timeout %u",
              base_, lifetime_pct_, bundle->expiration_, limit_, timeout);
    return timeout;
}

//----------------------------------------------------------------------
int
CustodyTimerSpec::parse_options(int argc, const char* argv[])
{
    oasys::OptParser p;
    p.addopt(new oasys::UIntOpt("custody_timer_base", &base_));
    p.addopt(new oasys::UIntOpt("custody_timer_lifetime_pct", &lifetime_pct_));
    p.addopt(new oasys::UIntOpt("custody_timer_limit", &limit_));
    return p.parse_and_shift(argc, argv);
}

//----------------------------------------------------------------------
CustodyTimer::CustodyTimer(const struct timeval& xmit_time,
                           const CustodyTimerSpec& spec,
                           Bundle* bundle, Link* link)
    : Logger("CustodyTimer", "/dtn/bundle/custody_timer"),
      bundle_(bundle, "CustodyTimer"), link_(link)
{
    struct timeval tv = xmit_time;
    u_int32_t delay = spec.calculate_timeout(bundle);
    tv.tv_sec += delay;

    struct timeval now;
    ::gettimeofday(&now, 0);
    log_info("scheduling timer: xmit_time %u.%u delay %u secs "
             "(in %lu msecs) for *%p",
             (u_int)xmit_time.tv_sec, (u_int)xmit_time.tv_usec, delay,
             TIMEVAL_DIFF_MSEC(tv, now), bundle);
    schedule_at(&tv);
}

//----------------------------------------------------------------------
void
CustodyTimer::timeout(const struct timeval& now)
{
    log_info("CustodyTimer::timeout");
    BundleDaemon::post(new CustodyTimeoutEvent(bundle_.object(), link_));
}

} // namespace dtn

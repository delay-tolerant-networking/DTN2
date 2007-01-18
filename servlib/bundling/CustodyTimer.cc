/*
 *    Copyright 2006 Intel Corporation
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
 * min: 30 minutes
 * lifetime percent: 25%
 * max: unlimited
 */
CustodyTimerSpec CustodyTimerSpec::defaults_(30 * 60, 25, 0);

//----------------------------------------------------------------------
u_int32_t
CustodyTimerSpec::calculate_timeout(const Bundle* bundle) const
{
    u_int32_t timeout = min_;
    timeout += (u_int32_t)((double)lifetime_pct_ * bundle->expiration_ / 100.0);

    if (max_ != 0) {
        timeout = std::min(timeout, max_);
    }
    
    log_debug_p("/dtn/bundle/custody_timer", "calculate_timeout: "
                "min %u, lifetime_pct %u, expiration %u, max %u: timeout %u",
                min_, lifetime_pct_, bundle->expiration_, max_, timeout);
    return timeout;
}

//----------------------------------------------------------------------
int
CustodyTimerSpec::parse_options(int argc, const char* argv[],
                                const char** invalidp)
{
    oasys::OptParser p;
    p.addopt(new oasys::UIntOpt("custody_timer_min", &min_));
    p.addopt(new oasys::UIntOpt("custody_timer_lifetime_pct", &lifetime_pct_));
    p.addopt(new oasys::UIntOpt("custody_timer_max", &max_));
    return p.parse_and_shift(argc, argv, invalidp);
}

//----------------------------------------------------------------------
CustodyTimer::CustodyTimer(const struct timeval& xmit_time,
                           const CustodyTimerSpec& spec,
                           Bundle* bundle, const LinkRef& link)
    : Logger("CustodyTimer", "/dtn/bundle/custody_timer"),
      bundle_(bundle, "CustodyTimer"), link_(link.object(), "CustodyTimer")
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
    (void)now;
    log_info("CustodyTimer::timeout");
    BundleDaemon::post(new CustodyTimeoutEvent(bundle_.object(), link_));
}

} // namespace dtn

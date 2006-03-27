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
#ifndef _CUSTODYTIMER_H_
#define _CUSTODYTIMER_H_

#include <oasys/thread/Timer.h>
#include "bundling/BundleRef.h"

namespace dtn {

class Bundle;
class Link;

/**
 * Utility class to abstract out various parameters that can be used
 * to calculate custody retransmission timers. This means that future
 * extensions that take into account other parameters or factors can
 * simply extend this class and modify the calculate_timeout()
 * function to add new features.
 *
 * The current basic scheme calculates the timer as:
 * <code>
 * timer = min(limit_, base_ + (lifetime_pct_ * bundle->lifetime_ / 100))
 * </code>
 *
 * In other words, this class allows a retransmisison to be specified
 * according to a minimum timer (base_), a multiplying factor based on
 * the bundle's lifetime (lifetime_pct_), and a maximum bound
 * (limit_). All values are in seconds.
 */
class CustodyTimerSpec {
public:
    /**
     * Custody timer defaults, values set in the static initializer.
     */
    static CustodyTimerSpec defaults_;
    
    /**
     * Constructor.
     */
    CustodyTimerSpec(u_int32_t base,
                     u_int32_t lifetime_pct,
                     u_int32_t limit)
        : base_(base), lifetime_pct_(lifetime_pct), limit_(limit) {}

    /**
     * Default Constructor.
     */
    CustodyTimerSpec()
        : base_(defaults_.base_),
          lifetime_pct_(defaults_.lifetime_pct_),
          limit_(defaults_.limit_) {}

    /**
     * Calculate the appropriate timeout for the given bundle.
     */
    u_int32_t calculate_timeout(const Bundle* bundle) const;

    /**
     * Parse options to set the fields of the custody timer. Shifts
     * any non-matching options to the beginning of the vector by
     * using OptParser::parse_and_shift.
     *
     * @return the number of parsed options
     */
    int parse_options(int argc, const char* argv[]);

    u_int32_t base_;		///< base timer
    u_int32_t lifetime_pct_;	///< percentage of lifetime
    u_int32_t limit_;		///< upper bound
};

/**
 * A custody transfer timer. Each bundle stores a vector of these
 * timers, as the bundle may be in flight on multiple links
 * concurrently, each having distinct retransmission timer parameters.
 * When a given timer fires, a retransmission is expected to be
 * initiated on only one link.
 * 
 * When a successful custody signal is received, the daemon will
 * cancel all timers and release custody, informing the routers as
 * such. If a failed custody signal is received or a timer fires, it
 * is up to the router to initiate a retransmission on one or more
 * links.
 */
class CustodyTimer : public oasys::Timer, public oasys::Logger {
public:
    /** Constructor */
    CustodyTimer(const struct timeval& xmit_time,
                 const CustodyTimerSpec& spec,
                 Bundle* bundle, Link* link);

    /** Virtual timeout function */
    void timeout(const struct timeval& now);

    ///< The bundle for whom the the timer refers
    BundleRef bundle_;

    ///< The link that it was transmitted on
    Link* link_;
};

/**
 * Class for a vector of custody timers.
 */
class CustodyTimerVec : public std::vector<CustodyTimer*> {};

} // namespace dtn

#endif /* _CUSTODYTIMER_H_ */

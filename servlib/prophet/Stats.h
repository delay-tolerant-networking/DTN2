/*
 *    Copyright 2007 Baylor University
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

#ifndef _PROPHET_STATS_H_
#define _PROPHET_STATS_H_

#include <map>
#include "Bundle.h"

namespace prophet
{

/**
 * Statistics to gather per Bundle as described in 
 * section 3.7 regarding Queuing Policies
 */
struct StatsEntry
{
    StatsEntry()
        : p_max_(0.0), mopr_(0.0), lmopr_(0.0) {}

    double p_max_;
    double mopr_;
    double lmopr_;
}; // StatsEntry

/**
 * Container for Bundle statistics, indexed by Bundle identifier.
 * Not thread-safe, requires external locking mechanism.
 */
class Stats
{
public:
    /**
     * Default constructor
     */
    Stats()
        : dropped_(0) {}

    /**
     * Destructor
     */
    ~Stats();

    /**
     * Given a Bundle and a predictability value, update
     * the stats kept for that Bundle
     */
    void update_stats(const Bundle* b, double p);

    /**
     * Given a Bundle, return the max predictability for
     * any route over which this Bundle has been forwarded
     */
    double get_p_max(const Bundle* b) const;

    /**
     * Given a Bundle, return the predictability favor for
     * the routes over which this Bundle has been forwarded,
     * according to Eq. 7, Section 3.7
     */
    double get_mopr(const Bundle* b) const;

    /**
     * Given a Bundle, return the linear predictability favor
     * for the routes over which this Bundle has been forwarded,
     * according to Eq. 8, Section 3.7
     */ 
    double get_lmopr(const Bundle* b) const;

    /**
     * Bundle is no longer with us, so get rid of its stats
     */
    void drop_bundle(const Bundle* b);

    /**
     * Return count of how many Bundle stats have been dropped
     * so far
     */
    u_int dropped() const { return dropped_; }

    /**
     * Return count of Bundles currently rep'd in Stats
     */
    size_t size() const { return pstats_.size(); }

protected:
    typedef std::map<u_int32_t,StatsEntry*> pstats;
    typedef std::map<u_int32_t,StatsEntry*>::iterator iterator;
    typedef std::map<u_int32_t,StatsEntry*>::const_iterator
        const_iterator;

    /**
     * Convenience function for finding the StatEntry per bundle id
     */
    StatsEntry* find(const Bundle* b);

    u_int  dropped_;
    mutable pstats pstats_;
}; // Stats

}; // namespace prophet

#endif // _PROPHET_STATS_H_

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

#ifndef _PROPHET_DECIDER_H_
#define _PROPHET_DECIDER_H_

#include "Bundle.h"
#include "Link.h"
#include "Stats.h"
#include "FwdStrategy.h"
#include "BundleCore.h"

namespace prophet
{

/**
 * Base class for FwdStrategy deciders, used by router to decide whether
 * to forward a Bundle based on the forwarding strategy
 */
class Decider
{
public:
    /**
     * Destructor
     */
    virtual ~Decider() {}

    /**
     * Factory method for creating decider instance
     */
    static inline Decider* decider(FwdStrategy::fwd_strategy_t fs,
                                   const Link* nexthop,
                                   BundleCore* core,
                                   const Table* local_nodes,
                                   const Table* remote_nodes,
                                   const Stats* stats = NULL,
                                   u_int max_forward = 0,
                                   bool is_relay = true);

    /**
     * The decision function: return true if the strategy says to forward
     * the Bundle; else false
     */
    virtual bool operator() (const Bundle*) const = 0;

    ///@{ Accessors
    FwdStrategy::fwd_strategy_t fwd_strategy() const { return fwd_strategy_; }
    const Link* nexthop() const { return next_hop_; }
    const BundleCore* core() const { return core_; }
    const Table* local_nodes() const { return local_; }
    const Table* remote_nodes() const { return remote_; }
    const Stats* stats() const { return stats_; }
    bool is_relay() const { return is_relay_; }
    ///@}

protected:
    /**
     * Constructor
     */
    Decider(FwdStrategy::fwd_strategy_t fs,
            const Link* nexthop,
            BundleCore* core,
            const Table* local, const Table* remote,
            const Stats* stats, bool is_relay)
        : fwd_strategy_(fs),
          next_hop_(nexthop),
          core_(core), local_(local), remote_(remote),
          stats_(stats), is_relay_(is_relay) {}

    FwdStrategy::fwd_strategy_t fwd_strategy_; ///< which strategy is in use
    const Link* next_hop_; ///< next hop Link
    BundleCore* const core_; ///< facade interface to Bundle host
    const Table* local_; ///< local routing table
    const Table* remote_; ///< peer's routing table
    const Stats* stats_; ///< forwarding statistics per Bundle
    bool is_relay_; ///< whether peer acts as a relay (forwards Bundles)

}; // class Decider

/**
 * Forward the bundle only if P(B,D) > P(A,D). That is, predictability
 * for route via peer is greater than route's predictability locally
 */
class FwdDeciderGRTR : public Decider
{
public:
    /**
     * Destructor
     */
    virtual ~FwdDeciderGRTR() {}

    /**
     * Virtual from Decider
     */
    bool operator() (const Bundle*) const;

protected:
    friend class Decider; // for factory method

    /**
     * Constructor.  Protected to force entry via factory method.
     */
    FwdDeciderGRTR(FwdStrategy::fwd_strategy_t fs,
                   const Link* nexthop, BundleCore* core,
                   const Table* local, const Table* remote,
                   const Stats* stats = NULL,
                   bool relay = true);
}; // class FwdDeciderGRTR

/**
 * Forward the bundle only if P(B,D) > P(A,D) (same as GRTR) and if the
 * bundle has been forwarded (NF) less than max times (NF_Max).
 */
class FwdDeciderGTMX : public FwdDeciderGRTR 
{
public:
    /**
     * Destructor
     */
    virtual ~FwdDeciderGTMX() {}

    /**
     * Virtual from Decider
     */
    bool operator() (const Bundle*) const;

    ///@{ Accessors
    u_int max_forward() const { return max_fwd_; }
    ///@}
protected:
    friend class Decider; // for factory method

    /**
     * Constructor.  Protected to force entry via factory method.
     */
    FwdDeciderGTMX(FwdStrategy::fwd_strategy_t fs,
                   const Link* nexthop, BundleCore* core,
                   const Table* local, const Table* remote,
                   u_int max_fwd, bool relay);

    u_int max_fwd_; ///< local configuration setting for NF_max
}; // class FwdDeciderGTMX

/**
 * Forward the bundle only if P(B,D) > P(A,D) (same as GRTR) and if 
 * P(B,D) > P_max where P_max is the largest delivery predictability 
 * reported by a node to which the bundle has been sent so far.
 */
class FwdDeciderGRTRPLUS : public FwdDeciderGRTR 
{
public:
    /**
     * Destructor
     */
    virtual ~FwdDeciderGRTRPLUS() {}

    /**
     * Virtual from Decider
     */
    bool operator() (const Bundle*) const;

protected:
    friend class Decider; // for factory method

    /**
     * Constructor.  Protected to force entry via factory method.
     */
    FwdDeciderGRTRPLUS(FwdStrategy::fwd_strategy_t fs,
                       const Link* nexthop, BundleCore* core,
                       const Table* local, const Table* remote,
                       const Stats* stats, bool relay);

}; // class FwdDeciderGRTRPLUS

/**
 * Forward the bundle only if
 * P(B,D) > P(A,D) && P(B,D) > P_max && NF < NF_max
 * which is a combination of GTMX and GRTR_PLUS
 */
class FwdDeciderGTMXPLUS : public FwdDeciderGRTRPLUS
{
public:
    /**
     * Destructor
     */
    virtual ~FwdDeciderGTMXPLUS() {}

    /**
     * Virtual from Decider
     */
    bool operator() (const Bundle*) const;

    ///@{ Accessors
    u_int max_forward() const { return max_fwd_; }
    ///@}
protected:
    friend class Decider; // for factory method

    /**
     * Constructor.  Protected to force entry via factory method.
     */
    FwdDeciderGTMXPLUS(FwdStrategy::fwd_strategy_t fs,
                       const Link* nexthop, BundleCore* core,
                       const Table* local, const Table* remote,
                       const Stats* stats,
                       u_int max_forward, bool relay);

    u_int max_fwd_; ///< local configuration setting for NF_max
}; // class FwdDeciderGTMXPLUS

Decider*
Decider::decider(FwdStrategy::fwd_strategy_t fs,
                 const Link* nexthop,
                 BundleCore* core,
                 const Table* local_nodes,
                 const Table* remote_nodes,
                 const Stats* stats,
                 u_int max_forward,
                 bool is_relay)
{
    Decider* d = NULL;

    // weed out the oddball
    if (nexthop == NULL || nexthop->nexthop()[0] == '\0') return NULL;
    if (local_nodes == NULL) return NULL;
    if (remote_nodes == NULL) return NULL;

    // pick the decider based on strategy
    switch (fs)
    {
        case FwdStrategy::GRTR:
        case FwdStrategy::GRTR_SORT:
        case FwdStrategy::GRTR_MAX:
        {
            d = new FwdDeciderGRTR(fs,nexthop,core,local_nodes,
                                   remote_nodes,NULL,is_relay);
            break;
        }
        case FwdStrategy::GTMX:
        {
            if (max_forward == 0) return NULL;
            d = new FwdDeciderGTMX(fs,nexthop,core,local_nodes,
                                   remote_nodes,max_forward,
                                   is_relay);
            break;
        }
        case FwdStrategy::GRTR_PLUS:
        {
            if (stats == NULL) return NULL;
            d = new FwdDeciderGRTRPLUS(fs,nexthop,core,local_nodes,
                                       remote_nodes,stats,is_relay);
            break;
        }
        case FwdStrategy::GTMX_PLUS:
        {
            if (stats == NULL || max_forward == 0) return NULL;
            d = new FwdDeciderGTMXPLUS(fs,nexthop,core,local_nodes,
                                       remote_nodes,stats,max_forward,
                                       is_relay);
            break;
        }
        case FwdStrategy::INVALID_FS:
        default:
            break;
    }
    return d;
}

}; // namespace prophet

#endif // _PROPHET_DECIDER_H_

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

#ifndef _PROPHET_PARAMS_H_
#define _PROPHET_PARAMS_H_

#include <sys/types.h>
#include "Node.h"
#include "FwdStrategy.h"
#include "QueuePolicy.h"

namespace prophet
{

/**
 * Tunable parameter struct for setting global default values
 * for various Prophet algorithms
 */
class ProphetParams : public NodeParams
{
public:
    /**
     * Time between HELLO beacons (in 100ms units)
     */
    static const u_int8_t HELLO_INTERVAL = 20; ///< 2 sec (arbitrary)

    /**
     * Max units of HELLO_INTERVAL before peer is considered unreachable
     */
    static const u_int HELLO_DEAD     = 20; ///< 40 sec (arbitrary)

    /**
     * Max times to forward a bundle for GTMX
     */
    static const u_int DEFAULT_NUM_F_MAX = 5; ///< arbitrary

    /**
     * Min times to forward a bundle for LEPR
     */
    static const u_int DEFAULT_NUM_F_MIN = 3; ///< arbitrary

    /**
     * Seconds between aging of nodes and Prophet ACKs
     */
    static const u_int AGE_PERIOD = 180; ///< 3 min (arbitrary)

    /**
     * Current version of the protocol
     */
    static const u_int8_t PROPHET_VERSION = 0x01;

    /**
     * Maximum number of routes to retain (not specified by I-D)
     */
    static const u_int MAX_TABLE_SIZE = 1024;

    /**
     * Constructor. Default values are defined in Prophet.h
     */
    ProphetParams()
        : NodeParams(),
          fs_(FwdStrategy::GRTR),
          qp_(QueuePolicy::FIFO),
          hello_interval_(HELLO_INTERVAL),
          hello_dead_(HELLO_DEAD),
          max_forward_(DEFAULT_NUM_F_MAX),
          min_forward_(DEFAULT_NUM_F_MIN),
          age_period_(AGE_PERIOD),
          max_table_size_(MAX_TABLE_SIZE),
          epsilon_(0.0039), // min value using 1/255
          relay_node_(true),
          internet_gw_(false) // not implemented
    {}

    ///@{ Accessors
    FwdStrategy::fwd_strategy_t fs() const { return fs_; }
    QueuePolicy::q_policy_t     qp() const { return qp_; }
    u_int8_t hello_interval() const { return hello_interval_; }
    u_int    hello_dead() const { return hello_dead_; }
    u_int    max_forward() const { return max_forward_; }
    u_int    min_forward() const { return min_forward_; }
    u_int    age_period() const { return age_period_; }
    double   epsilon() const { return epsilon_; }
    bool relay_node() const { return relay_node_; }
    bool internet_gw() const { return internet_gw_; }
    ///@}

    FwdStrategy::fwd_strategy_t fs_; ///< bundle forwarding strategy
    QueuePolicy::q_policy_t     qp_; ///< bundle queuing policy

    u_int8_t hello_interval_; ///< delay between HELLO beacons (100ms units)
    u_int    hello_dead_;     ///< hello_interval's before giving up on peer

    u_int    max_forward_; ///< max times to forward bundle using GTMX
    u_int    min_forward_; ///< min times to forward bundle before LEPR drops

    u_int    age_period_; ///< seconds between applying age algorithm

    u_int    max_table_size_; ///< max number of routes to retain

    double   epsilon_;    ///< minimum predictability before dropping route

    bool relay_node_; ///< whether this node accepts bundles to relay to peers
    bool internet_gw_; ///< not implemented; whether node bridges to Internet
};

}; // namespace prophet

#endif // _PROPHET_PARAMS_H_

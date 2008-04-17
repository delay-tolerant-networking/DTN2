/*
 *    Copyright 2007 Intel Corporation
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

#ifndef _DTLSR_CONFIG_H_
#define _DTLSR_CONFIG_H_

#include <string>
#include <oasys/compat/inttypes.h>
#include <oasys/util/Options.h>
#include <oasys/util/Singleton.h>

namespace dtn {

/**
 * Class to encapsulate the DTLSR configuration
 */
class DTLSRConfig : public oasys::Singleton<DTLSRConfig> {
public:
    /**
     * Constructor to initialize defaults.
     */
    DTLSRConfig();

    /**
     * Administratively assigned area.
     */
    std::string area_;

    /**
     * Configurable weight function types.
     */
    typedef enum {
        COST,
        DELAY,
        ESTIMATED_DELAY
    } weight_fn_t;

    /**
     * Stringified version of the weight function.
     */
    static const char* weight_fn_to_str(weight_fn_t f);

    /**
     * Options array for setting the weight function using an oasys option.
     */
    static oasys::EnumOpt::Case weight_opts_[];
        
    /**
     * Configurable weight function.
     */
    weight_fn_t weight_fn_;

    /**
     * Configurable scaling on link weight.
     */
    u_int weight_shift_;
        
    /**
     * Factor by which to age the cost of a link based on its
     * uptime. The default is 10.0, i.e. as a link's uptime goes
     * to zero, the cost increases by 10x the original cost.
     */
    double uptime_factor_;

    /**
     * Whether or not to keep down links in the graph, marking
     * them as stale.
     */
    bool keep_down_links_;

    /**
     * Delay (in seconds) after receiving an LSA when we recompute
     * the routes. Needed to prevent some flapping.
     *
     * XXX/demmer not used
     */
    u_int recompute_delay_;

    /**
     * Interval (in seconds) after which we locally recompute the
     * routes to properly age links that we believe to be down.
     */
    u_int aging_interval_;
        
    /**
     * Interval (in seconds) at which we proactively send new LSAa.
     * Default is once per hour.
     */
    u_int lsa_interval_;

    /**
     * Minimum interval (in seconds) between LSA transmission. Default
     * is once per five seconds.
     */
    u_int min_lsa_interval_;
    
    /**
     * Expiration time for lsa announcements (default is
     * infinite).
     */
    u_int lsa_lifetime_;
};

} // namespace dtn

#endif /* _DTLSR_CONFIG_ */

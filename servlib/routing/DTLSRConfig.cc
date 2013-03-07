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

#ifdef HAVE_CONFIG_H
#include <dtn-config.h>
#endif

#include "DTLSRConfig.h"

template <>
dtn::DTLSRConfig* oasys::Singleton<dtn::DTLSRConfig>::instance_ = NULL;

namespace dtn {

//----------------------------------------------------------------------
DTLSRConfig::DTLSRConfig()
    : area_(""),
      weight_fn_(ESTIMATED_DELAY),
      weight_shift_(0),
      uptime_factor_(10.0),
      keep_down_links_(true),
      recompute_delay_(1),
      aging_interval_(5),
      lsa_interval_(3600),
      min_lsa_interval_(5),
      lsa_lifetime_(24 * 3600)
{
}

//----------------------------------------------------------------------
const char*
DTLSRConfig::weight_fn_to_str(weight_fn_t fn)
{
    switch(fn) {
    case COST:            return "COST";
    case DELAY:           return "DELAY";
    case ESTIMATED_DELAY: return "ESTIMATED_DELAY";
    }
    return "INVALID_WEIGHT_FN";
}

//----------------------------------------------------------------------
oasys::EnumOpt::Case
DTLSRConfig::weight_opts_[] =
{
    {"cost",            COST},
    {"delay",           DELAY},
    {"estimated_delay", ESTIMATED_DELAY},
    {"estdelay",        ESTIMATED_DELAY},
    {NULL,              0},
};

} // namespace dtn

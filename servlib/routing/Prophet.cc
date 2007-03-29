/*
 *    Copyright 2006 Baylor University
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
#  include <config.h>
#endif

#include "Prophet.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

Prophet::UniqueID* Prophet::UniqueID::instance_ = NULL;

const double Prophet::DEFAULT_P_ENCOUNTER;
const double Prophet::DEFAULT_BETA;
const double Prophet::DEFAULT_GAMMA;

bool
Prophet::route_to_me(const EndpointID& eid)
{
    EndpointID local = BundleDaemon::instance()->local_eid();
    EndpointIDPattern route = eid_to_route(local);
    return route.match(eid);
}

} // dtn

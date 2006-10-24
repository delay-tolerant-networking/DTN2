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

#ifndef _ANNOUNCE_BUNDLE_H_
#define _ANNOUNCE_BUNDLE_H_

#include "Bundle.h"
#include "naming/EndpointID.h"

namespace dtn {

/**
 * Beacon sent by Neighbor Discovery element within Convergence Layer
 * to announce to other DTN nodes. 
 */
class AnnounceBundle
{
public:
    static void create_announce_bundle(Bundle *bundle,
                                       const EndpointID& route_eid);

    static bool parse_announce_bundle(Bundle *bundle,
                                      EndpointID *route_eid = NULL);
}; // AnnounceBundle

} // dtn

#endif // _ANNOUNCE_BUNDLE_H_

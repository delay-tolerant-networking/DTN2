/*
 *    Copyright 2006-2007 The MITRE Corporation
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
 *
 *    The US Government will not be charged any license fee and/or royalties
 *    related to this software. Neither name of The MITRE Corporation; nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

#include "ForwardingInfo.h"

namespace dtn {

void 
ForwardingInfo::serialize(oasys::SerializeAction *a)
{
    a->process("state", &state_);
    a->process("action", &action_);
    a->process("clayer", &clayer_);
    a->process("nextHop", &nexthop_);

    // casting won't be necessary after port to oasys::Time
    a->process("timestamp_sec",
        reinterpret_cast< u_int32_t * >(&timestamp_.tv_sec));
    a->process("timestamp_usec",
        reinterpret_cast< u_int32_t * >(&timestamp_.tv_usec));
}

} // namespace dtn

/*
 *    Copyright 2004-2006 Intel Corporation
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


#include "WildcardScheme.h"
#include "EndpointID.h"

namespace dtn {

template <>
WildcardScheme* oasys::Singleton<WildcardScheme>::instance_ = 0;

/**
 * Validate that the SSP in the given URI is legitimate for
 * this scheme. If the 'is_pattern' paraemeter is true, then
 * the ssp is being validated as an EndpointIDPattern.
 *
 * @return true if valid
 */
bool
WildcardScheme::validate(const URI& uri, bool is_pattern)
{
    // the wildcard scheme can only be used for patterns and must be
    // exactly the string "*"
    if ((is_pattern == false) || (uri.ssp() != "*")) {
        return false;
    }

    return true;
}
    
/**
 * Match the pattern to the endpoint id in a scheme-specific
 * manner.
 */
bool
WildcardScheme::match(const EndpointIDPattern& pattern,
                      const EndpointID& eid)
{
    (void)eid;
    ASSERT(pattern.scheme() == this); // sanity

    // XXX/demmer why was this here:
    
    // the only thing we don't match is the special null endpoint
//     if (eid.str() == "dtn:none") {
//         return false;
//     }

    return true;
}

} // namespace dtn


/* Copyright 2004-2006 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * $Id$
 */

#ifndef _GBOFID_H_
#define _GBOFID_H_

#include "naming/EndpointID.h"
#include "bundling/BundleTimestamp.h"
//#include "routing/router.h"

namespace dtn {

/**
 * Class definition for a GBOF ID (Global Bundle Or Fragment ID)
 */
class GbofId
{
public:
    GbofId();
    ~GbofId();

    /**
     * Compares if GBOF IDs are the same.
     */
    bool equals(GbofId);

    /** 
     * Compares if fields match those of this GBOF ID
     */
    bool equals(EndpointID,
                BundleTimestamp,
                bool,
                u_int32_t,
                u_int32_t);
                
    /**
     * Returns a string version of the gbof
     */
    std::string str();

    EndpointID source_;		  ///< Source eid
    BundleTimestamp creation_ts_; ///< Creation timestamp
    bool is_fragment_;		  ///< Fragmentary Bundle
    u_int32_t frag_length_;  	  ///< Length of original bundle
    u_int32_t frag_offset_;	  ///< Offset of fragment in original bundle
};

} // namespace dtn

#endif /* _GBOFID_H_ */

/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _BUNDLE_MAPPING_H_
#define _BUNDLE_MAPPING_H_

#include <oasys/debug/Debug.h>
#include "BundleList.h"

namespace dtn {

/**
 * Each time a bundle is put on a bundle list, a mapping is maintained
 * in the bundle structure containing information about the
 * association. This information can be used by the routers to make
 * more informed scheduling decisions.
 */
class BundleMappingInfo;
class RouterInfo;

/**
 * Various forwarding actions
 */
typedef enum {
    FORWARD_INVALID = 0,///< Invalid action
    
    FORWARD_UNIQUE,	///< Forward the bundle only to one next hop
    FORWARD_COPY,	///< Forward a copy of the bundle
    FORWARD_FIRST,	///< Forward to the first of a set
    FORWARD_REASSEMBLE	///< First reassemble fragments (if any) then forward
} bundle_fwd_action_t;

inline const char*
bundle_fwd_action_toa(bundle_fwd_action_t action)
{
    switch(action) {
    case FORWARD_UNIQUE:	return "FORWARD_UNIQUE";
    case FORWARD_COPY:		return "FORWARD_COPY";
    case FORWARD_FIRST:		return "FORWARD_FIRST";
    case FORWARD_REASSEMBLE:	return "FORWARD_REASSEMBLE";
    default:
        NOTREACHED;
    }
}

/**
 * Simple structure to encapsulate mapping state that's passed in when the
 * bundle is queued on a list.
 */
struct BundleMapping {
     BundleMapping()
        : action_(FORWARD_INVALID),
          mapping_grp_(0),
          timeout_(0),
          router_info_(NULL)
    {}
    
    BundleMapping(const BundleMapping& other)
        : action_(other.action_),
          mapping_grp_(other.mapping_grp_),
          timeout_(other.timeout_),
          router_info_(other.router_info_)
    {}

    BundleMapping(bundle_fwd_action_t action,
                  int mapping_grp,
                  u_int32_t timeout,
                  RouterInfo* router_info)        
        : action_(action),
          mapping_grp_(mapping_grp),
          timeout_(timeout),
          router_info_(router_info)
    {}

    BundleList::iterator position_;	///< Position of the bundle in the list
    bundle_fwd_action_t action_;	///< The forwarding action code
    int mapping_grp_;			///< Mapping group identifier
    u_int32_t timeout_;			///< Timeout for the mapping
    RouterInfo* router_info_;		///< Slot for router private state
};

} // namespace dtn

#endif /* _BUNDLE_MAPPING_H_ */

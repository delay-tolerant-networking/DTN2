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
#ifndef _BUNDLE_ACTION_H_
#define _BUNDLE_ACTION_H_

#include <vector>
#include "Bundle.h"
#include "BundleMapping.h"
#include "BundleRef.h"
#include "Link.h"

namespace dtn {

class BundleConsumer;

/**
 * Type code for bundling actions.
 */
typedef enum {
    ENQUEUE_BUNDLE,	///< Queue a bundle on a consumer
    DEQUEUE_BUNDLE,	///< Remove a bundle from a consumer

    STORE_ADD,		///< Store the bundle in the database
    STORE_DEL,		///< Remove the bundle from the database

    OPEN_LINK,          ///< Open link
} bundle_action_t;


/**
 * Conversion function from an action to a string.
 */
inline const char*
bundle_action_toa(bundle_action_t action)
{
    switch(action) {

    case ENQUEUE_BUNDLE:	return "ENQUEUE_BUNDLE";
    case DEQUEUE_BUNDLE:	return "DEQUEUE_BUNDLE";
        
    case STORE_ADD:		return "STORE_ADD";
    case STORE_DEL:		return "STORE_DEL";

    case OPEN_LINK:             return "OPEN_LINK";
        
    default:			return "(invalid action type)";
    }
}

/**
 * Structure encapsulating a generic bundle related action.
 */
class BundleAction {
public:
    BundleAction(bundle_action_t action, Bundle* bundle)
        : action_(action),
          bundleref_(bundle, "BundleAction", bundle_action_toa(action))
    {
    }

    const char* type_str() { return bundle_action_toa(action_); }
    bundle_action_t action_;	///< action type code
    BundleRef bundleref_;	///< relevant bundle

private:
    /**
     * Make sure the action copy constructor is never run by making it
     * private, since it would screw up the Bundle reference counts.
     */
    BundleAction(const BundleAction& other);
};
    
/**
 * The vector of actions that is constructed by the BundleRouter.
 * Define a (empty) class, not a typedef, so we can use forward
 * declarations.
 */
class BundleActionList : public std::vector<BundleAction*> {
};

/**
 * Structure encapsulating an enqueue action.
 */
class BundleEnqueueAction : public BundleAction {
public:
    BundleEnqueueAction(Bundle* bundle,
                        BundleConsumer* nexthop,
                        bundle_fwd_action_t fwdaction,
                        int mapping_grp = 0,
                        u_int32_t expiration = 0,
                        RouterInfo* router_info = 0)
        
        : BundleAction(ENQUEUE_BUNDLE, bundle),
          nexthop_(nexthop)
    {
        mapping_.action_ 	= fwdaction;
        mapping_.mapping_grp_ 	= mapping_grp;
        mapping_.timeout_ 	= expiration;
        mapping_.router_info_ 	= router_info;
    }

    BundleConsumer* nexthop_;		///< destination
    BundleMapping mapping_;		///< mapping structure
};

/**
 * Structure for a dequeue action.
 */
class BundleDequeueAction : public BundleAction {
    BundleDequeueAction(Bundle* bundle,
                        BundleConsumer* nexthop)
        : BundleAction(DEQUEUE_BUNDLE, bundle),
          nexthop_(nexthop)
    {
    }
        
    BundleConsumer* nexthop_;		///< destination
};

    
/**
 * Structure for a open link action
 * Done really need bundle, XXX/Sushant get rid of it
 */
class OpenLinkAction : public BundleAction {
public:
    OpenLinkAction(Bundle* bundle, Link* link)
        : BundleAction(OPEN_LINK,bundle),
          link_(link)
    {
    }
        
    Link* link_;		///< link
};


} // namespace dtn

#endif /* _BUNDLE_ACTION_H_ */

/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2006 Intel Corporation. All rights reserved. 
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

#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include "BundleRouter.h"
#include "RouteEntry.h"
#include "contacts/Link.h"

namespace dtn {

//----------------------------------------------------------------------
RouteEntry::RouteEntry(const EndpointIDPattern& pattern, Link* link)
    : pattern_(pattern),
      route_priority_(0),
      next_hop_(link),
      action_(FORWARD_UNIQUE),
      custody_timeout_(),
      info_(NULL)
{
    route_priority_ = BundleRouter::Config.default_priority_;
}

//----------------------------------------------------------------------
RouteEntry::~RouteEntry()
{
    if (info_)
        delete info_;
}

//----------------------------------------------------------------------
int
RouteEntry::parse_options(int argc, const char** argv, const char** invalidp)
{
    int num = custody_timeout_.parse_options(argc, argv, invalidp);
    if (num == -1) {
        return -1;
    }
    
    argc -= num;
    
    oasys::OptParser p;

    p.addopt(new oasys::UIntOpt("route_priority", &route_priority_));

    oasys::EnumOpt::Case fwdopts[] = {
        {"forward_unique", FORWARD_UNIQUE},
        {"forward_copy",   FORWARD_COPY},
	{0, 0}
    };
    p.addopt(new oasys::EnumOpt("action", fwdopts, (int*)&action_));

    int num2 = p.parse_and_shift(argc, argv, invalidp);
    if (num2 == -1) {
        return -1;
    }

    return num + num2;
}

//----------------------------------------------------------------------
void
RouteEntry::dump(oasys::StringBuffer* buf) const
{
    buf->appendf("%s -> %s (%s) route_priority %d "
                 "[custody timeout: base %u lifetime_pct %u limit %u]\n",
                 pattern_.c_str(),
                 next_hop_->name(),
                 bundle_fwd_action_toa(action_),
                 route_priority_,
                 custody_timeout_.base_,
                 custody_timeout_.lifetime_pct_,
                 custody_timeout_.limit_);
}

//----------------------------------------------------------------------
/**
 * Functor class to sort a vector by priority.
 */
struct RoutePriorityGT {
    bool operator() (RouteEntry* a, RouteEntry* b) {
        if (a->route_priority_ == b->route_priority_)
        {
            return (a->next_hop_->stats()->bytes_inflight_ <
                    b->next_hop_->stats()->bytes_inflight_);
        }
        
        return a->route_priority_ > b->route_priority_;
    }
};

//----------------------------------------------------------------------
void
RouteEntryVec::sort_by_priority()
{
    std::sort(begin(), end(), RoutePriorityGT());
}

} // namespace dtn

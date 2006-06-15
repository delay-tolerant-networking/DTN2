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
#include "naming/EndpointIDOpt.h"

namespace dtn {

//----------------------------------------------------------------------
RouteEntry::RouteEntry(const EndpointIDPattern& dest_pattern, Link* link)
    : dest_pattern_(dest_pattern),
      source_pattern_(EndpointID::WILDCARD_EID()),
      bundle_cos_((1 << Bundle::COS_BULK) |
                  (1 << Bundle::COS_NORMAL) |
                  (1 << Bundle::COS_EXPEDITED)),
      route_priority_(0),
      next_hop_(link),
      action_(ForwardingInfo::FORWARD_ACTION),
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

    p.addopt(new EndpointIDOpt("source_eid", &source_pattern_));
    p.addopt(new oasys::UIntOpt("route_priority", &route_priority_));
    p.addopt(new oasys::UIntOpt("cos_flags", &bundle_cos_));
    oasys::EnumOpt::Case fwdopts[] = {
        {"forward", ForwardingInfo::FORWARD_ACTION},
        {"copy",    ForwardingInfo::COPY_ACTION},
	{0, 0}
    };
    p.addopt(new oasys::EnumOpt("action", fwdopts, (int*)&action_));

    int num2 = p.parse_and_shift(argc, argv, invalidp);
    if (num2 == -1) {
        return -1;
    }
    
    if ((bundle_cos_ == 0) || (bundle_cos_ >= (1 << 3))) {
        static const char* s = "invalid cos flags";
        invalidp = &s;
        return -1;
    }

    return num + num2;
}

//----------------------------------------------------------------------
int
RouteEntry::format(char* bp, size_t sz) const
{
    // XXX/demmer when the route table is serialized, add an integer
    // id for the route entry and include it here.
    return snprintf(bp, sz, "%s -> %s (%s)",
                    dest_pattern_.c_str(),
                    next_hop_->nexthop(),
                    ForwardingInfo::action_to_str(action_));
}

//----------------------------------------------------------------------
void
RouteEntry::dump_header(oasys::StringBuffer* buf)
{
    // though this is a less efficient way of appending this data, it
    // makes it much easier to copy the format string to the ::dump
    // method, and given that it's only used for diagnostics, the
    // performance impact is negligible
    buf->appendf("%-15s %-10s %3s    %-13s %-7s %5s [%-15s]\n"
                 "%-15s %-10s %3s    %-13s %-7s %5s [%-5s %-3s %-5s]\n"
                 "-----------------------------------"
                 "-------------------------------------------\n",
                 "destination",
                 " source",
                 "COS",
                 "link",
                 " fwd  ",
                 "route",
                 "custody timeout",
                 " endpoint",
                 "endpoint",
                 "BNE",
                 "name",
                 "action",
                 "prio",
                 "min",
                 "pct",
                 "  max");
}

//----------------------------------------------------------------------
void
RouteEntry::dump(oasys::StringBuffer* buf, EndpointIDVector* long_eids) const
{
    size_t len;
    if (dest_pattern_.length() <= 15) {
        buf->appendf("%-15s ", dest_pattern_.c_str());
    } else {
        len = buf->appendf("[%u]", long_eids->size());
        buf->appendf("%.*s", 16 - len, "               ");
        long_eids->push_back(dest_pattern_);
    }
    
    if (source_pattern_.length() <= 10) {
        buf->appendf("%-10s ", source_pattern_.c_str());
    } else {
        len = buf->appendf("[%u]", long_eids->size());
        buf->appendf("%.*s", 11 - len, "               ");
        long_eids->push_back(source_pattern_);
    }
    
    buf->appendf("%c%c%c -> %-13.13s %7s %5d [%-5d %3d %5d]\n",
                 (bundle_cos_ & (1 << Bundle::COS_BULK))      ? '1' : '0',
                 (bundle_cos_ & (1 << Bundle::COS_NORMAL))    ? '1' : '0',
                 (bundle_cos_ & (1 << Bundle::COS_EXPEDITED)) ? '1' : '0',
                 next_hop_->name(),
                 ForwardingInfo::action_to_str(action_),
                 route_priority_,
                 custody_timeout_.min_,
                 custody_timeout_.lifetime_pct_,
                 custody_timeout_.max_);
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

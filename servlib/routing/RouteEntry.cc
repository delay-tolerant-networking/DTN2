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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include "BundleRouter.h"
#include "RouteEntry.h"
#include "naming/EndpointIDOpt.h"

namespace dtn {

//----------------------------------------------------------------------
RouteEntry::RouteEntry(const EndpointIDPattern& dest_pattern,
                       const LinkRef& link)
    : dest_pattern_(dest_pattern),
      source_pattern_(EndpointIDPattern::WILDCARD_EID()),
      bundle_cos_((1 << Bundle::COS_BULK) |
                  (1 << Bundle::COS_NORMAL) |
                  (1 << Bundle::COS_EXPEDITED)),
      route_priority_(0),
      next_hop_(link.object(), "RouteEntry"),
      action_(ForwardingInfo::FORWARD_ACTION),
      custody_timeout_(), // XXX/demmer rename to custody_spec
      info_(NULL)
{
    route_priority_ = BundleRouter::config_.default_priority_;
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
    p.addopt(new oasys::EnumOpt("action", fwdopts, &action_));

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
                    ForwardingInfo::action_to_str(
                        static_cast<ForwardingInfo::action_t>(action_)));
}

static const char* DASHES =
    "---------------------------------------------"
    "---------------------------------------------"
    "---------------------------------------------"
    "---------------------------------------------"
    "---------------------------------------------"
    "---------------------------------------------";

//----------------------------------------------------------------------
void
RouteEntry::dump_header(int dest_eid_limit, int source_eid_limit,
                        oasys::StringBuffer* buf)
{
    // though this is a less efficient way of appending this data, it
    // makes it much easier to copy the format string to the ::dump
    // method, and given that it's only used for diagnostics, the
    // performance impact is negligible
    buf->appendf("%-*.*s %-*.*s %3s    %-13s %-7s %5s [%-15s]\n"
                 "%-*.*s %-*.*s %3s    %-13s %-7s %5s [%-5s %-3s %-5s]\n"
                 "%.*s\n",
                 dest_eid_limit, dest_eid_limit, "destination",
                 source_eid_limit, source_eid_limit, " source",
                 "COS",
                 "link",
                 " fwd  ",
                 "route",
                 "custody timeout",
                 
                 dest_eid_limit, dest_eid_limit, " endpoint",
                 source_eid_limit, source_eid_limit, "endpoint",
                 "BNE",
                 "name",
                 "action",
                 "prio",
                 "min",
                 "pct",
                 "  max",
                 
                 dest_eid_limit + source_eid_limit + 65,
                 DASHES);
}

//----------------------------------------------------------------------
void
RouteEntry::dump(int dest_eid_limit, int source_eid_limit,
                 oasys::StringBuffer* buf, EndpointIDVector* long_eids) const
{
    size_t len;
    char tmp[64];
    if (dest_pattern_.length() <= (size_t)dest_eid_limit) {
        buf->appendf("%-*.*s ", dest_eid_limit, dest_eid_limit,
                     dest_pattern_.c_str());
    } else {
        len = snprintf(tmp, sizeof(tmp), "[%zu] ", long_eids->size());
        buf->appendf("%.*s... %s",
                     dest_eid_limit - 3 - (int)len,
                     dest_pattern_.c_str(), tmp);
        long_eids->push_back(dest_pattern_);
    }
    
    if (source_pattern_.length() <= (size_t)source_eid_limit) {
        buf->appendf("%-*.*s ", source_eid_limit, source_eid_limit,
                     source_pattern_.c_str());
    } else {
        len = snprintf(tmp, sizeof(tmp), "[%zu] ", long_eids->size());
        buf->appendf("%.*s... %s",
                     source_eid_limit - 3 - (int)len,
                     source_pattern_.c_str(), tmp);
        long_eids->push_back(source_pattern_);
    }
    
    buf->appendf("%c%c%c -> %-13.13s %7s %5d [%-5d %3d %5d]\n",
                 (bundle_cos_ & (1 << Bundle::COS_BULK))      ? '1' : '0',
                 (bundle_cos_ & (1 << Bundle::COS_NORMAL))    ? '1' : '0',
                 (bundle_cos_ & (1 << Bundle::COS_EXPEDITED)) ? '1' : '0',
                 next_hop_->name(),
                 ForwardingInfo::action_to_str(
                    static_cast<ForwardingInfo::action_t>(action_)),
                 route_priority_,
                 custody_timeout_.min_,
                 custody_timeout_.lifetime_pct_,
                 custody_timeout_.max_);
}

//----------------------------------------------------------------------
void
RouteEntry::serialize(oasys::SerializeAction *a)
{
    a->process("dest_pattern", &dest_pattern_);
    a->process("source_pattern", &source_pattern_);
    a->process("route_priority", &route_priority_);
    a->process("action", &action_);
    a->process("link", const_cast<std::string *>(&next_hop_->name_str()));
}

} // namespace dtn

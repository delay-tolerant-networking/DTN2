/*
 *    Copyright 2007 Baylor University
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

#include "Decider.h"

#define LOG(_args...) if (core_ != NULL) core_->print_log("decider", \
        BundleCore::LOG_DEBUG, _args)

namespace prophet
{

FwdDeciderGRTR::FwdDeciderGRTR(FwdStrategy::fwd_strategy_t fs,
                               const Link* nexthop,
                               BundleCore* core,
                               const Table* local,
                               const Table* remote,
                               const Stats* stats,
                               bool relay)
    : Decider(fs,nexthop,core,local,remote,stats,relay) {}

bool
FwdDeciderGRTR::operator()(const Bundle* b) const
{
    // weed out the oddball
    if (b == NULL) return false; 

    // ask the core about status first
    if (!core_->should_fwd(b,next_hop_))
    {
        LOG("core declined to fwd %s %u:%u",
                b->destination_id().c_str(),
                b->creation_ts(), b->sequence_num());
        return false;
    }
    
    if (core_->is_route(b->destination_id(),next_hop_->remote_eid()))
    {
        LOG("ok to fwd: %s is destination for bundle %s %u:%u",
            next_hop_->remote_eid(),b->destination_id().c_str(),
            b->creation_ts(),b->sequence_num());
        return true;
    }
    // if no route match and remote is not a relay,
    // don't bother forwarding this bundle
    else if (!is_relay_)
        return false;

    std::string dest_eid = core_->get_route(b->destination_id());
    // forward if remote's predictability is greater than local's
    if (local_->p_value(dest_eid) < remote_->p_value(dest_eid))
    {
        LOG("ok to fwd: %s is better route for bundle %s %u:%u "
            "(%.2f > %.2f)", next_hop_->remote_eid(),b->destination_id().c_str(),
            b->creation_ts(),b->sequence_num(),
            remote_->p_value(b), local_->p_value(b));
        return true;
    }

    // no reason to forward
    return false;
}

FwdDeciderGTMX::FwdDeciderGTMX(FwdStrategy::fwd_strategy_t fs,
                               const Link* nexthop,
                               BundleCore* core,
                               const Table* local,
                               const Table* remote,
                               u_int max_fwd,
                               bool relay)
    : FwdDeciderGRTR(fs,nexthop,core,local,remote,NULL,relay),
      max_fwd_(max_fwd) {}

bool
FwdDeciderGTMX::operator()(const Bundle* b) const
{
    // defer first refusal to base class 
    if (! FwdDeciderGRTR::operator()(b))
        return false;
    
    if (core_->is_route(b->destination_id(),next_hop_->nexthop()))
    {
        LOG("ok to fwd: %s is destination for bundle %s %u:%u",
            next_hop_->remote_eid(),b->destination_id().c_str(),
            b->creation_ts(),b->sequence_num());
        return true;
    }
    // if no route match and remote is not a relay,
    // don't bother forwarding this bundle
    else if (!is_relay_)
        return false;

    // Check max fwd
    if (b->num_forward() < max_fwd_)
    {
        LOG("ok to fwd: num_fwd(bundle %s %u:%u) == %u, max == %u",
            b->destination_id().c_str(), b->creation_ts(),b->sequence_num(),
            b->num_forward(), max_fwd_);
        return true;
    }

    // Do not forward
    return false;
}

FwdDeciderGRTRPLUS::FwdDeciderGRTRPLUS(FwdStrategy::fwd_strategy_t fs,
                                       const Link* nexthop,
                                       BundleCore* core,
                                       const Table* local,
                                       const Table* remote,
                                       const Stats* stats,
                                       bool relay)
    : FwdDeciderGRTR(fs,nexthop,core,local,remote,stats,relay) {}

bool
FwdDeciderGRTRPLUS::operator()(const Bundle* b) const
{
    // defer first refusal to base class 
    if (! FwdDeciderGRTR::operator()(b))
        return false;
    
    if (core_->is_route(b->destination_id(),next_hop_->nexthop()))
    {
        LOG("ok to fwd: %s is destination for bundle %s %u:%u",
            next_hop_->remote_eid(),b->destination_id().c_str(),
            b->creation_ts(),b->sequence_num());
        return true;
    }
    // if no route match and remote is not a relay,
    // don't bother forwarding this bundle
    else if (!is_relay_)
        return false;

    // forward only if remote's P is greater than local's P_max
    bool ok = stats_->get_p_max(b) < remote_->p_value(b);
    if (ok)
        LOG("ok to fwd: remote p (%.2f) greater than max p (%.2f) for "
            "%s %u:%u", remote_->p_value(b), stats_->get_p_max(b),
            b->destination_id().c_str(), b->creation_ts(),
            b->sequence_num());
    return ok;
}

FwdDeciderGTMXPLUS::FwdDeciderGTMXPLUS(FwdStrategy::fwd_strategy_t fs,
                                       const Link* nexthop,
                                       BundleCore* core,
                                       const Table* local,
                                       const Table* remote,
                                       const Stats* stats,
                                       u_int max_forward,
                                       bool relay)
    : FwdDeciderGRTRPLUS(fs,nexthop,core,local,remote,stats,relay),
      max_fwd_(max_forward) {}

bool
FwdDeciderGTMXPLUS::operator()(const Bundle* b) const
{
    // defer first refusal to base class 
    if (! FwdDeciderGRTRPLUS::operator()(b))
        return false;
    
    if (core_->is_route(b->destination_id(),next_hop_->nexthop()))
    {
        LOG("ok to fwd: %s is destination for bundle %s %u:%u",
            next_hop_->remote_eid(),b->destination_id().c_str(),
            b->creation_ts(),b->sequence_num());
        return true;
    }

    // if no route match and remote is not a relay,
    // don't bother forwarding this bundle
    else if (!is_relay_)
        return false;

    bool num_fwd_ok = b->num_forward() < max_fwd_;
    bool p_max_ok = stats_->get_p_max(b) < remote_->p_value(b);

    if (num_fwd_ok && p_max_ok)
        LOG("ok to fwd: NF (%u) < maxNF (%u) and remote p (%.2f) > "
            "max (%.2f) for %s %u:%u",b->num_forward(), max_fwd_,
            remote_->p_value(b), stats_->get_p_max(b),
            b->destination_id().c_str(), b->creation_ts(),
            b->sequence_num());

    return num_fwd_ok && p_max_ok;
}

}; // namespace prophet

/*
 *    Copyright 2005-2006 Intel Corporation
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


#include <oasys/thread/SpinLock.h>
#include <oasys/util/StringBuffer.h>
#include "conv_layers/ConvergenceLayer.h"
#include "ForwardingLog.h"

namespace dtn {

//----------------------------------------------------------------------
ForwardingLog::ForwardingLog(oasys::SpinLock* lock)
    : lock_(lock)
{
}

//----------------------------------------------------------------------
bool
ForwardingLog::get_latest_entry(const LinkRef& link, ForwardingInfo* info) const
{
    oasys::ScopeLock l(lock_, "ForwardingLog::get_latest_state");
    
    Log::const_reverse_iterator iter;
    for (iter = log_.rbegin(); iter != log_.rend(); ++iter)
    {
        if (iter->nexthop_ == link->nexthop() &&
            iter->clayer_  == link->clayer()->name())
        {
            *info = *iter;
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------
ForwardingLog::state_t
ForwardingLog::get_latest_entry(const LinkRef& link) const
{
    ForwardingInfo info;
    if (! get_latest_entry(link, &info)) {
        return ForwardingInfo::NONE;
    }

    return static_cast<ForwardingLog::state_t>(info.state_);
}

//----------------------------------------------------------------------
size_t
ForwardingLog::get_transmission_count(ForwardingInfo::action_t action,
                                      bool include_inflight) const
{
    size_t ret = 0;
    
    oasys::ScopeLock l(lock_, "ForwardingLog::get_transmission_count");
    
    Log::const_iterator iter;
    for (iter = log_.begin(); iter != log_.end(); ++iter)
    {
        if (iter->state_ == ForwardingInfo::TRANSMITTED ||
            (include_inflight && (iter->state_ == ForwardingInfo::IN_FLIGHT)))
        {
            if ((action == iter->action_) ||
                (action == ForwardingInfo::INVALID_ACTION))
            {
                ++ret;
            }
        }
    }
    
    return ret;
}

//----------------------------------------------------------------------
size_t
ForwardingLog::get_count(state_t state) const
{
    size_t ret = 0;

    oasys::ScopeLock l(lock_, "ForwardingLog::get_count");
    
    Log::const_iterator iter;
    for (iter = log_.begin(); iter != log_.end(); ++iter)
    {
        if (iter->state_ == state) {
            ++ret;
        }
    }

    return ret;
}

//----------------------------------------------------------------------
void
ForwardingLog::dump(oasys::StringBuffer* buf) const
{
    oasys::ScopeLock l(lock_, "ForwardingLog::dump");
    buf->appendf("forwarding log:\n");
    Log::const_iterator iter;
    for (iter = log_.begin(); iter != log_.end(); ++iter)
    {
        const ForwardingInfo* info = &(*iter);
        
        buf->appendf("\t%s -> %s %u.%u [%s cl:%s] [custody min %d pct %d max %d]\n",
                     ForwardingInfo::state_to_str(
                        static_cast<ForwardingInfo::state_t>(info->state_)),
                     info->clayer_.c_str(),
                     (u_int)info->timestamp_.tv_sec,
                     (u_int)info->timestamp_.tv_usec,
                     ForwardingInfo::action_to_str(
                        static_cast<ForwardingInfo::action_t>(info->action_)),
                     info->nexthop_.c_str(),
                     info->custody_timer_.min_,
                     info->custody_timer_.lifetime_pct_,
                     info->custody_timer_.max_);
    }
}
    
//----------------------------------------------------------------------
void
ForwardingLog::add_entry(const LinkRef& link,
                         ForwardingInfo::action_t action,
                         state_t state,
                         const CustodyTimerSpec& custody_timer)
{
    oasys::ScopeLock l(lock_, "ForwardingLog::add_entry");
    
    log_.push_back(ForwardingInfo(state, action, link->clayer()->name(),
                                  link->nexthop(), custody_timer));
}

//----------------------------------------------------------------------
bool
ForwardingLog::update(const LinkRef& link, state_t state)
{
    oasys::ScopeLock l(lock_, "ForwardingLog::update");
    
    Log::reverse_iterator iter;
    for (iter = log_.rbegin(); iter != log_.rend(); ++iter)
    {
        if (iter->nexthop_ == link->nexthop() &&
            iter->clayer_  == link->clayer()->name())
        {
            iter->set_state(state);
            return true;
        }
    }
    
    return false;
}

} // namespace dtn

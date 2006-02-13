/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
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

#include <oasys/thread/SpinLock.h>
#include <oasys/util/StringBuffer.h>
#include "conv_layers/ConvergenceLayer.h"
#include "contacts/Link.h"
#include "ForwardingLog.h"

namespace dtn {

//----------------------------------------------------------------------
ForwardingLog::ForwardingLog(oasys::SpinLock* lock)
    : lock_(lock)
{
}

//----------------------------------------------------------------------
bool
ForwardingLog::get_latest_entry(Link* link, ForwardingInfo* info) const
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
        
        buf->appendf("\t%s -> %s %u.%u [%s cl:%s] [custody base %d pct %d limit %d]\n",
                     ForwardingInfo::state_to_str(info->state_),
                     info->clayer_.c_str(),
                     (u_int)info->timestamp_.tv_sec,
                     (u_int)info->timestamp_.tv_usec,
                     bundle_fwd_action_toa(info->action_),
                     info->nexthop_.c_str(),
                     info->custody_timer_.base_,
                     info->custody_timer_.lifetime_pct_,
                     info->custody_timer_.limit_);
    }
}
    
//----------------------------------------------------------------------
void
ForwardingLog::add_entry(Link* link,
                         bundle_fwd_action_t action,
                         state_t state,
                         const CustodyTimerSpec& custody_timer)
{
    log_.push_back(ForwardingInfo(state, action, link->clayer()->name(),
                                  link->nexthop(), custody_timer));
}

//----------------------------------------------------------------------
bool
ForwardingLog::update(Link* link, state_t state)
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

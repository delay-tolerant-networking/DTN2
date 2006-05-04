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

#include "Registration.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "storage/GlobalStore.h"

namespace dtn {

const char*
Registration::failure_action_toa(failure_action_t action)
{
    switch(action) {
    case DROP:  return "DROP";
    case DEFER:	return "DEFER";
    case EXEC:  return "EXEC";
    }

    return "__INVALID__";
}

Registration::Registration(u_int32_t regid,
                           const EndpointIDPattern& endpoint,
                           failure_action_t action,
                           u_int32_t expiration,
                           const std::string& script)
    
    : Logger("Registration", "/dtn/registration/%d", regid),
      regid_(regid),
      endpoint_(endpoint),
      failure_action_(action),
      script_(script),
      expiration_(expiration),
      expiration_timer_(NULL),
      active_(false),
      expired_(false)
{
    struct timeval now;
    ::gettimeofday(&now, 0);
    creation_time_ = now.tv_sec;
    
    init_expiration_timer();
}

/**
 * Constructor for deserialization
 */
Registration::Registration(const oasys::Builder&)
    : Logger("Registration", "/dtn/registration"),
      regid_(0),
      endpoint_(),
      failure_action_(DEFER),
      script_(),
      expiration_(0),
      creation_time_(0),
      expiration_timer_(NULL),
      active_(false),
      expired_(false)
{
}

/**
 * Destructor.
 */
Registration::~Registration()
{
    if (expiration_timer_) {
        // try to cancel the expiration timer. if it is still pending,
        // then we're being deleted due to a user action from either
        // the console or a call to dtn_unregister, and the timer will
        // clean itself up when it eventually fires. otherwise, assert
        // that we have actually expired and delete the timer itself
        bool pending = expiration_timer_->cancel();

        if (! pending) {
            ASSERT(expired_);
            delete expiration_timer_;
        }
    }
}

/**
 * Virtual from SerializableObject.
 */
void
Registration::serialize(oasys::SerializeAction* a)
{
    a->process("endpoint", &endpoint_);
    a->process("regid", &regid_);
    a->process("action", (u_int32_t*)&failure_action_);
    a->process("script", &script_);
    a->process("creation_time", &creation_time_);
    a->process("expiration", &expiration_);

    // finish constructing the object after unserialization
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        init_expiration_timer();
    }

    logpathf("/dtn/registration/%d", regid_);
}

void
Registration::init_expiration_timer()
{
    // XXX/demmer need to actually store the creation time as well so
    // the expiration time doesn't get reset every time we start up
    
    if (expiration_ != 0) {
        struct timeval when, now;
        when.tv_sec  = creation_time_ + expiration_;
        when.tv_usec = 0;

        ::gettimeofday(&now, 0);

        long int in_how_long = TIMEVAL_DIFF_MSEC(when, now);
        if (in_how_long < 0) {
            log_warn("scheduling IMMEDIATE expiration for registration id %d: "
                     "[creation_time %u, expiration %u, now %u]", 
                     regid_, creation_time_, expiration_, (u_int)now.tv_sec);
        } else {
            log_debug("scheduling expiration for registration id %d at %u.%u "
                      "(in %ld seconds): ", regid_,
                      (u_int)when.tv_sec, (u_int)when.tv_usec, in_how_long / 1000);
        }
        
        expiration_timer_ = new ExpirationTimer(this);
        expiration_timer_->schedule_at(&when);

    } else {
        set_expired(true);
    }
}

void
Registration::ExpirationTimer::timeout(const struct timeval& now)
{
    reg_->set_expired(true);
                      
    if (! reg_->active()) {
        BundleDaemon::post(new RegistrationExpiredEvent(reg_->regid()));
    } 
}

} // namespace dtn

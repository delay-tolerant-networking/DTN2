/*
 *    Copyright 2004-2006 Intel Corporation
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


#include "Registration.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "storage/GlobalStore.h"

namespace dtn {

//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
Registration::Registration(u_int32_t regid,
                           const EndpointIDPattern& endpoint,
                           int action,
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

//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
Registration::~Registration()
{
    cleanup_expiration_timer();
}

//----------------------------------------------------------------------
void
Registration::force_expire()
{
    ASSERT(active_);
    
    cleanup_expiration_timer();
    set_expired(true);
}

//----------------------------------------------------------------------
void
Registration::cleanup_expiration_timer()
{
    if (expiration_timer_) {
        // try to cancel the expiration timer. if it is still pending,
        // then the timer will clean itself up when it eventually
        // fires. otherwise, assert that we have actually expired and
        // delete the timer itself.
        bool pending = expiration_timer_->cancel();
        
        if (! pending) {
            ASSERT(expired_);
            delete expiration_timer_;
        }
        
        expiration_timer_ = NULL;
    }
}

//----------------------------------------------------------------------
void
Registration::serialize(oasys::SerializeAction* a)
{
    a->process("endpoint", &endpoint_);
    a->process("regid", &regid_);
    a->process("action", &failure_action_);
    a->process("script", &script_);
    a->process("creation_time", &creation_time_);
    a->process("expiration", &expiration_);

    // finish constructing the object after unserialization
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        init_expiration_timer();
    }

    logpathf("/dtn/registration/%d", regid_);
}

//----------------------------------------------------------------------
void
Registration::init_expiration_timer()
{
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
                      (u_int)when.tv_sec, (u_int)when.tv_usec,
                      in_how_long / 1000);
        }
        
        expiration_timer_ = new ExpirationTimer(this);
        expiration_timer_->schedule_at(&when);

    } else {
        set_expired(true);
    }
}

//----------------------------------------------------------------------
void
Registration::ExpirationTimer::timeout(const struct timeval& now)
{
    (void)now;
    
    reg_->set_expired(true);
                      
    if (! reg_->active()) {
        BundleDaemon::post(new RegistrationExpiredEvent(reg_->regid()));
    } 
}

} // namespace dtn

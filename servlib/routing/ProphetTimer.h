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

#ifndef _PROPHET_TIMER_H_
#define _PROPHET_TIMER_H_

#include <oasys/thread/Timer.h>
#include <oasys/thread/Lock.h>
#include "prophet/Alarm.h"
#include "prophet/Encounter.h"

namespace dtn
{

class ProphetTimer : public oasys::Timer,
                     public prophet::Alarm
{
public:
    ProphetTimer(prophet::ExpirationHandler* eh,
                 oasys::SpinLock* lock)
    : oasys::Timer(),
      prophet::Alarm(eh),
      lock_(lock)
    {
        ASSERT( eh != NULL );
        ASSERT( lock != NULL );
    }

    virtual ~ProphetTimer() {}

    /**
     * Virtual from oasys::Timer
     */
    void timeout(const struct timeval& now)
    {
        (void)now;
        oasys::ScopeLock l(lock_,"ProphetTimer::timeout");
        prophet::Alarm::timeout();
    }

    ///@{ Virtual from prophet::Alarm
    void schedule(u_int milliseconds)
    {
        oasys::Timer::schedule_in(milliseconds);
    }
    u_int time_remaining() const
    {
        struct timeval now;
        ::gettimeofday(&now,0);
        struct timeval when = const_cast<ProphetTimer*>(this)->when();
        int diff = TIMEVAL_DIFF_MSEC(when, now);
        return (diff < 0) ? 0 : diff;
    }
    void cancel() { oasys::Timer::cancel(); }
    bool pending() const { return oasys::Timer::pending_; }
    bool cancelled() const { return oasys::Timer::cancelled_; }
    ///@}

protected:
    oasys::SpinLock* lock_; ///< synchronize/serialize access to handler

}; // class ProphetTimer

}; // namespace dtn

#endif // _PROPHET_TIMER_H_

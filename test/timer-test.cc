
#include <stdio.h>
#include <unistd.h>
#include "thread/Timer.h"
#include "debug/Log.h"

class PeriodicTimer : public Timer {
  public:
    PeriodicTimer(const char* name) {
        snprintf(log_, sizeof(log_), "/timer/%s", name);
        logf(log_, LOG_DEBUG, "PeriodicTimer 0x%x", (u_int)this);
    }
    
    void timeout(struct timeval* now) {
        int late = TIMEVAL_DIFF_USEC(*now, when());
        logf(log_, LOG_INFO, "timer at %ld.%ld (%d usec late)",
             now->tv_sec, now->tv_usec, late);
        
        reschedule();
    }
    
    virtual void reschedule() = 0;
    
  protected:
    char log_[64];
};

class TenSecondTimer : public PeriodicTimer {
  public:
    TenSecondTimer() : PeriodicTimer("10sec") { reschedule(); }
    void reschedule() { schedule_in(10000); }
};
               
class OneSecondTimer : public PeriodicTimer {
  public:
    OneSecondTimer() : PeriodicTimer("1sec") { reschedule(); }
    void reschedule() { schedule_in(1000); }
};
               
class HalfSecondTimer : public PeriodicTimer {
  public:
    HalfSecondTimer() : PeriodicTimer("500msec") { reschedule(); }
    void reschedule() { schedule_in(500); }
};

class TenImmediateTimer : public PeriodicTimer {
public:
    TenImmediateTimer() : PeriodicTimer("10immed")
    {
        count_ = 0;
        reschedule();
    }
    void reschedule() {
        if (count_ == 0) {
            count_ = 10;
            schedule_in(1000);
        } else {
            count_--;
            schedule_in(1);
        }
    }
protected:
    int count_;
};

               
int main()
{
    Log::init(LOG_INFO);
    TimerSystem::init();

    logf("/timertest", LOG_INFO, "timer test initializing");

    new OneSecondTimer();

    new TenSecondTimer();
    new OneSecondTimer();
    new HalfSecondTimer();

    usleep(500000);
    new OneSecondTimer();

//     new HalfSecondTimer();
//     usleep(100000);
//     new HalfSecondTimer();
//     usleep(100000);
//     new HalfSecondTimer();
//     usleep(100000);
//     new HalfSecondTimer();

    new TenImmediateTimer();

    sleep(50000);
}

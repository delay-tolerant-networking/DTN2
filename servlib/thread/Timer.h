#ifndef tier_timer_h
#define tier_timer_h

#include <sys/types.h>
#include <sys/time.h>

#include "debug/Debug.h"
#include "debug/Log.h"
#include "MsgQueue.h"
#include "Thread.h"
#include <queue>

/**
 * Miscellaneous timeval macros.
 */
#define TIMEVAL_DIFF(t1, t2, t3) \
    do { \
       ((t3).tv_sec  = (t1).tv_sec - (t2).tv_sec); \
       ((t3).tv_usec = (t1).tv_usec - (t2).tv_usec); \
       if ((t3).tv_usec < 0) { (t3).tv_sec--; (t3).tv_usec += 1000000; } \
    } while (0)

#define TIMEVAL_DIFF_DOUBLE(t1, t2) \
    ((double)(((t1).tv_sec  - (t2).tv_sec)) + \
     (double)((((t1).tv_usec - (t2).tv_usec)) * 1000000.0))

#define TIMEVAL_DIFF_MSEC(t1, t2) \
    ((((t1).tv_sec  - (t2).tv_sec)  * 1000) + \
     (((t1).tv_usec - (t2).tv_usec) / 1000))

#define TIMEVAL_DIFF_USEC(t1, t2) \
    ((((t1).tv_sec  - (t2).tv_sec)  * 1000000) + \
     (((t1).tv_usec - (t2).tv_usec)))

#define TIMEVAL_GT(t1, t2) \
    (((t1).tv_sec  >  (t2).tv_sec) ||  \
     (((t1).tv_sec == (t2).tv_sec) && ((t1).tv_usec > (t2).tv_usec)))

#define TIMEVAL_LT(t1, t2) \
    (((t1).tv_sec  <  (t2).tv_sec) ||  \
     (((t1).tv_sec == (t2).tv_sec) && ((t1).tv_usec < (t2).tv_usec)))

class SpinLock;
class Timer;

/**
 * The Timer comparison class.
 */
class TimerCompare {
public:
    inline bool operator ()(Timer* a, Timer* b);
};    

/**
 * The main Timer system implementation.
 */
class TimerSystem : public Thread, public Logger {
public:
    static TimerSystem* instance() {
        ASSERT(instance_ != NULL);
        return instance_;
    }

    static void init();

    void schedule_at(struct timeval *when, Timer* timer);
    void schedule_in(int milliseconds, Timer* timer);
    void schedule_immediate(Timer* timer);
    bool cancel(Timer* timer);

    void run();

private:
    TimerSystem();
    
    void pop_timer(struct timeval *now);
    
    static TimerSystem* instance_;

    SpinLock* system_lock_;
    MsgQueue<char> signal_;
    std::priority_queue<Timer*, std::vector<Timer*>, TimerCompare> timers_;
};

/**
 * A Timer class. Provides methods for scheduling timers. Derived
 * classes must override the pure virtual timeout() method.
 */
class Timer {
public:
    Timer() : pending_(false), cancelled_(false), delete_on_cancel_(false) {}
    
    virtual ~Timer() {}
    
    void schedule_at(struct timeval *when)
    {
        TimerSystem::instance()->schedule_at(when, this);
    }
    
    void schedule_in(int milliseconds)
    {
        TimerSystem::instance()->schedule_in(milliseconds, this);
    }

    void schedule_immediate()
    {
        TimerSystem::instance()->schedule_immediate(this);
    }
    
    bool cancel(bool delete_on_cancel = false)
    {
        delete_on_cancel_ = delete_on_cancel;
        return TimerSystem::instance()->cancel(this);
    }

    int pending()
    {
        return pending_;
    }

    struct timeval when()
    {
        return when_;
    }
    
    virtual void timeout(struct timeval* now) = 0;

private:
    friend class TimerSystem;
    friend class TimerCompare;
    
    struct timeval when_;
    bool pending_;
    bool cancelled_;
    bool delete_on_cancel_;
};

/**
 * The Timer comparator function used in the priority queue.
 */
bool
TimerCompare::operator()(Timer* a, Timer* b)
{
    return TIMEVAL_GT(a->when_, b->when_);
}

/**
 * For use with the QueuingTimer, this struct defines a TimerEvent,
 * i.e. a particular firing of a Timer that captures the timer and the
 * time when it fired.
 */
struct TimerEvent {
    TimerEvent(const Timer* timer, struct timeval* time)
        : timer_(timer), time_(*time)
    {
    }
    
    const Timer* timer_;
    struct timeval time_;
};

/**
 * The queue type used in the QueueingTimer.
 */
typedef MsgQueue<TimerEvent> TimerEventQueue;
    
/**
 * A Timer class that's useful in cases when a separate thread (i.e.
 * not the main TimerSystem thread) needs to process the timer event.
 * Note that multiple QueuingTimer instances can safely share the same
 * event queue.
 */
class QueuingTimer : public Timer {
public:
    QueuingTimer(TimerEventQueue* queue) : queue_(queue) {}
    
    virtual void timeout(struct timeval* now)
    {
        queue_->push(TimerEvent(this, now));
    }
    
protected:
    TimerEventQueue* queue_;
};

#endif /* tier_timer_h */


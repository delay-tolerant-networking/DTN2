#ifndef TIMER_H
#define TIMER_H

#include <sys/types.h>
#include <sys/time.h>

class Timer {
public:
    Timer() {}
    virtual ~Timer() {} 

    /**
     * Virtual timeout function, called on the object when the timer
     * fires.
     */
    virtual void timeout(struct timeval* now) = 0;

    /**
     * Schedule a timer to go off at a given time.
     */
    int schedule_at(struct timeval *when);

    /**
     * Schedule a timer to go off after the given interval.
     */
    int schedule_in(int milliseconds);

    /**
     * Schedule a timer to go off immediately, albeit from the event
     * dispatch thread and not the current thread.
     */
    int schedule_immediate();
    
    /**
     * Cancel a previously scheduled timer.
     */
    int cancel();

    /**
     * Return true if the Timer is currently pending.
     */
    bool pending();
    
    static inline int compare(Timer* a, Timer* b);

protected:
    friend class EventSystem;
    
    struct timeval when_;
    int pending_:1;
    int cancelled_:1;
};

int
Timer::compare(Timer* a, Timer* b)
{
    if (a->when_.tv_sec < b->when_.tv_sec) {
        return 1;
    }
    else if (a->when_.tv_sec > b->when_.tv_sec) {
        return -1;
    }
    else {
        // repeat with usec
        if (a->when_.tv_usec < b->when_.tv_usec) {
            return 1;
        } else if (a->when_.tv_usec > b->when_.tv_usec) {
            return -1;
        }
    }
    
    return 0;
}

#endif /* TIMER_H */


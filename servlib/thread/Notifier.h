#ifndef _NOTIFIER_H_
#define _NOTIFIER_H_

#include "debug/Log.h"

class SpinLock;

/**
 * Thread notification abstraction that wraps an underlying pipe. This
 * can be used as a generic abstraction to pass messages between
 * threads.
 *
 * A call to notify() sends a byte down the pipe, while calling wait()
 * causes the thread to block in poll() waiting for some data to be
 * sent on the pipe.
 *
 * In addition, through read_fd() a thread can get the file descriptor
 * to explicitly block on while awaiting notification. In that case,
 * the calling thread should explicitly call drain_pipe().
 */
class Notifier : public Logger {
public:
    Notifier(const char* logpath = NULL);
    ~Notifier();

    /**
     * Block the calling thread, pending a call to notify(). If a lock
     * is passed in, wait() will unlock the lock before the thread
     * blocks and re-take it when the thread unblocks.
     */
    void wait(SpinLock* lock = NULL);

    /**
     * Return indication if there are any blocked threads.
     */
    bool has_waiter() { return waiter_; }
    
    /**
     * Notify a waiter.
     */
    void notify();

    /**
     * Drain the pipe. Never blocks.
     */
    void drain_pipe();

    /**
     * The read side of the pipe, suitable for an explicit call to
     * poll().
     */
    int read_fd() { return pipe_[0]; }

    /**
     * The write side of the pipe.
     */
    int write_fd() { return pipe_[1]; }
    
protected:
    int pipe_[2];
    bool waiter_;
};

#endif /* _NOTIFIER_H_ */

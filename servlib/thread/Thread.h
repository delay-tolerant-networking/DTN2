#ifndef _THREAD_H_
#define _THREAD_H_

#include <pthread.h>
#include <signal.h>
#include "debug/Debug.h"

/*
 * Base class for classes that "are" threads. Similar to the Java API.
 */
class Thread {
public:
    /**
     * Bit values for thread flags.
     */
    enum thread_flags_t {
        CREATE_DETACHED	= 1 << 0,	///< same as PTHREAD_CREATE_DETACHED
        DELETE_ON_EXIT  = 1 << 1,	///< delete thread when run() completes
        INTERRUPTABLE   = 1 << 2,	///< thread can be interrupted
        SHOULD_STOP   	= 1 << 3,	///< bit to signal the thread to stop
        STOPPED   	= 1 << 4,	///< bit indicating the thread has stopped
    };

    /**
     * Constructor / Destructor
     */
    Thread(int flags = 0);
    virtual ~Thread();
    
    /**
     * Starts a new thread and calls the virtual run() method with the
     * new thread.
     */
    void start();

    /**
     * Join with this thread, blocking the caller until we quit.
     */
    void join();

    /**
     * Yield the current process. Needed for capriccio to yield the
     * processor during long portions of code with no I/O. (Probably
     * used for testing only).
     */
    static void yield();

    /**
     * Potentially yield the current thread to implement a busy
     * waiting function.
     *
     * The implementation is dependant on the configuration. On an
     * SMP, don't do anything. Otherwise, with capriccio threads, this
     * should never happen, so issue an assertion. Under pthreads,
     * however, actually call thread_yield() to give up the processor.
     */
    static inline void spin_yield();

    /**
     * Return a pointer to the currently running thread.
     *
     * XXX/demmer this could keep a map to return a Thread* if it's
     * actually useful
     */
    static pthread_t current();

    /**
     * Send a signal to the thread.
     */
    void kill(int sig);

    /**
     * Send an interrupt signal to the thread to unblock it if it's
     * stuck in a system call. Implemented with SIGUSR1.
     */
    void interrupt();

    /**
     * Set the interruptable state of the thread (default off). If
     * interruptable, a thread can receive SIGUSR1 signals to
     * interrupt any system calls.
     *
     * Note: This must be called by the thread itself. To set the
     * interruptable state before a thread is created, use the
     * INTERRUPTABLE flag.
     */
    void set_interruptable(bool interruptable);
    
    /**
     * Set a bit to stop the thread
     *
     * This simply sets a bit in the thread's flags that can be
     * examined by the run method to indicate that the thread should
     * be stopped.
     */
    void set_should_stop() { flags_ |= SHOULD_STOP; }

    /**
     * Return whether or not the thread should stop.
     */
    bool should_stop() { return ((flags_ & SHOULD_STOP) != 0); }
    
    /**
     * Return true if the thread has stopped.
     */
    bool is_stopped() { return ((flags_ & STOPPED) != 0); }

    /**
     * Set the given thread flag.
     */
    void set_flag(thread_flags_t flag) { flags_ |= flag; }
    
    /**
     * Return a pointer to the Thread object's id. Note that this may
     * or may not be the currently executing thread.
     */
    pthread_t thread_id()
    {
        return pthread_;
    }
    
protected:
    /**
     * Derived classes should implement this function which will get
     * called in the new Thread context.
     */
    virtual void run() = 0;

    static void* thread_run(void* t);
    static void interrupt_signal(int sig);

    pthread_t pthread_;
    int flags_;

    static bool signals_inited_;
    static sigset_t interrupt_sigset_;
};

inline pthread_t
Thread::current()
{
    return pthread_self();
}

inline void
Thread::yield()
{
    pthread_yield();
}

void
Thread::spin_yield()
{
#ifdef NO_SMP
#ifdef _POSIX_THREAD_IS_CAPRICCIO
    PANIC("SpinLock should never be contended under capriccio");
#else
    Thread::yield();
#endif // _POSIX_THREAD_IS_CAPRICCIO
#endif // NO_SMP
}

#endif /* _THREAD_H_ */

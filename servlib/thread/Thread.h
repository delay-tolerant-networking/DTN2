#ifndef _THREAD_H_
#define _THREAD_H_

#include <pthread.h>
#include "debug/Debug.h"

/*
 * Base class for classes that "are" threads. Similar to the Java API.
 */
class Thread {
public:
    /*!
     * Bit values for thread creation flags.
     */
    enum ThreadFlags {
        CREATE_DETACHED	= 1 << 0,	// same as PTHREAD_CREATE_DETACHED
        DELETE_ON_EXIT  = 1 << 1	// delete thread when run() completes
    };

    /*!
     * Constructor / Destructor
     */
    Thread(int flags = 0);
    virtual ~Thread();
    
    /*!
     * Starts a new thread and calls the virtual run() method with the
     * new thread.
     */
    void start();

    /*!
     * Join with this thread, blocking the caller until we quit.
     */
    void join();

    /*!
     * Yield the current process. Needed for capriccio to yield the
     * processor during long portions of code with no I/O. (Probably
     * used for testing only.
     */
    static void yield();

    /*!
     * Potentially yield the current thread to implement a busy
     * waiting function.
     *
     * The implementation is dependant on the configuration. On an
     * SMP, don't do anything. Otherwise, with capriccio threads, this
     * should never happen, so issue an assertion. Under pthreads,
     * however, actually call thread_yield() to give up the processor.
     */
    static inline void spin_yield();

    /*!
     * Return a pointer to the currently running thread.
     *
     * XXX/demmer this could keep a map to return a Thread* if it's
     * actually useful
     */
    static pthread_t current();

    /*!
     * Return a pointer to the Thread object's id. Note that this may
     * or may not be the currently executing thread.
     */
    pthread_t thread_id()
    {
        return pthread_;
    }
    
protected:
    /*!
     * Derived classes should implement this function which will get
     * called in the new Thread context.
     */
    virtual void run() = 0;

    static void* thread_run(void* t);

    pthread_t pthread_;
    int flags_;
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
#ifdef SMP // don't do anything
#else
#ifdef _POSIX_THREAD_IS_CAPRICCIO
    PANIC("SpinLock should never be contended under capriccio");
#else
    Thread::yield();
#endif // _POSIX_THREAD_IS_CAPRICCIO
#endif // SMP
}

#endif /* _THREAD_H_ */

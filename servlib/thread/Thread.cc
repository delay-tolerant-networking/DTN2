
#include "Thread.h"
#include "debug/Debug.h"
#include "debug/Log.h"

bool Thread::signals_inited_ = false;
sigset_t Thread::interrupt_sigset_;

void
Thread::interrupt_signal(int sig)
{
}

void*
Thread::thread_run(void* t)
{
    Thread* thr = (Thread*)t;

    /*
     * There's a potential race between the starting of the new thread
     * and the storing of the thread id in the pthread_ member
     * variable, so we can't trust that it's been written by our
     * spawner. So we re-write it here to make sure that it's valid
     * for the new thread (specifically for set_interruptable's
     * assertion.
     */
    thr->pthread_ = Thread::current();
    
    thr->set_interruptable((thr->flags_ & INTERRUPTABLE));

    thr->flags_ &= ~STOPPED;
    thr->flags_ &= ~SHOULD_STOP;
    
    thr->run();
    
    thr->flags_ |= STOPPED;
    
    if (thr->flags_ & DELETE_ON_EXIT) {
        delete thr;
    }
    return 0;
}

Thread::Thread(int flags)
    : flags_(flags)
{
}

Thread::~Thread()
{
}

void
Thread::start()
{
    // if this is the first thread, set up signals
    if (!signals_inited_) {
        sigemptyset(&interrupt_sigset_);
        sigaddset(&interrupt_sigset_, SIGUSR1);
        signal(SIGUSR1, interrupt_signal);
        siginterrupt(SIGUSR1, 1);
        signals_inited_ = true;
    }
    
    // XXX implement create_detached
    ASSERT(! (flags_ & CREATE_DETACHED));
    
    if (pthread_create(&pthread_, 0, Thread::thread_run, this) != 0) {
        PANIC("error in pthread_create");
    }
}

void
Thread::join()
{
    void* ignored;
    if (pthread_join(pthread_, &ignored) != 0) {
        PANIC("error in pthread_join");
    }
}

void
Thread::kill(int sig)
{
    if (pthread_kill(pthread_, sig) != 0) {
        PANIC("error in pthread_kill");
    }
}

void
Thread::interrupt()
{
    kill(SIGUSR1);
}

void
Thread::set_interruptable(bool interruptable)
{
    ASSERT(Thread::current() == pthread_);
    
    int block = (interruptable ? SIG_UNBLOCK : SIG_BLOCK);
    if (pthread_sigmask(block, &interrupt_sigset_, NULL) != 0) {
        PANIC("error in pthread_sigmask");
    }
}

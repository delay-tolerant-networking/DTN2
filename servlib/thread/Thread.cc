
#include "Thread.h"
#include "Debug.h"
#include "Log.h"

void*
Thread::thread_run(void* t)
{
    Thread* thr = (Thread*)t;
    thr->run();
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
    // XXX implement create_detached
    ASSERT(! (flags_ & CREATE_DETACHED));
    
    if (pthread_create(&pthread_, 0, Thread::thread_run, this) != 0) {
        ASSERT(0);
    }
}

void
Thread::join()
{
    void* ignored;
    if (pthread_join(pthread_, &ignored) != 0) {
        ASSERT(0);
    }
}

#include <unistd.h>
#include <errno.h>

#include "lib/Debug.h"
#include "lib/Log.h"
#include "Mutex.h"


Mutex::Mutex(const char* name, lock_type_t type, bool keep_quiet)
    : Lock(), type_(type), keep_quiet_(keep_quiet)
{
    // This hackish assignment is here because C99 syntax has diverged
    // from the C++ standard! Woohoo!
    switch(type_)
    {
    case TYPE_FAST: {
        pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
        mutex_ = m;
        break;
    }

    case TYPE_RECURSIVE: {
#ifdef _POSIX_THREAD_IS_CAPRICCIO
        // Capriccio is by default recursively enabled
        pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
#else
        pthread_mutex_t m = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif
        mutex_ = m;
        break;
    }}

    if (name[0] == '/')
        ++name;
    
    if (strcmp(name, "lock") == 0) 
    {
        logpathf("/mutex/%s(%p)", name, this);
    }
    else
    {
        logpathf("/mutex/%s", name);
    }
}

Mutex::~Mutex()
{
    pthread_mutex_destroy(&mutex_);
    logf(LOG_DEBUG, "destroyed");
}

int
Mutex::lock()
{
    int err = pthread_mutex_lock(&mutex_);
    
    logf(LOG_DEBUG,
         "%s %d",
         (err == 0) ? "locked" : "error on locking",
         err);
    
    if (err == 0)
        lock_holder_ = Thread::current();

    return err;
}

int
Mutex::unlock()
{
    int err = pthread_mutex_unlock(&mutex_);
    
    logf(LOG_DEBUG, 
         "%s %d",
         (err == 0) ? "unlocked" : "error on unlocking",
         err);

    if (err == 0)
        lock_holder_ = Thread::current();
    
    return err;
}

int
Mutex::try_lock()
{
    int err = pthread_mutex_trylock(&mutex_);

    if (err == EBUSY) 
    {
        logf(LOG_DEBUG, "try_lock busy");
    }

    logf(LOG_DEBUG, 
         "%s %d",
         (err == 0) ? "locked" : "error on try_lock",
         err);

    if (err == 0)
        lock_holder_ = Thread::current();
    
    return err;
}


/**
 * Since the Log class uses a Mutex internally, calling logf from
 * within lock() / unlock() would lead to infinite recursion, hence we
 * use the keep_quiet_ flag to suppress all logging for this instance.
 */
int
Mutex::logf(log_level_t level, const char *fmt, ...)
{
    if (keep_quiet_) return 0;
    
    va_list ap;
    va_start(ap, fmt);
    int ret = vlogf(level, fmt, ap);
    va_end(ap);
    return ret;
}



#ifndef _MUTEX_H_
#define _MUTEX_H_

#include <pthread.h>
#include "Lock.h"
#include "Logger.h"

/// Mutex wrapper class for pthread_mutex_t.
class Mutex : public Lock, public Logger {
    friend class Monitor; // Monitor needs access to mutex_.
    
public:
    /// Different kinds of mutexes offered by Linux, distinguished by
    /// their response to a single thread attempting to acquire the
    /// same lock more than once. For Capriccio, fast and recursive
    /// are all the same.
    ///
    /// - FAST: No error checking. The thread will block forever.
    /// - RECURSIVE: Double locking is safe.
    ///
    enum lock_type_t {
        TYPE_FAST = 1,
        TYPE_RECURSIVE
    };

    /// Creates a mutex. By default, we create a TYPE_RECURSIVE.
    Mutex(const char* name = "lock", 
          lock_type_t type = TYPE_RECURSIVE,
          bool keep_quiet = false);
    ~Mutex();

    /// Aquire mutex.
    int lock();

    /// Release mutex.
    int unlock();

    /// Try to acquire a lock. If already locked, fail.
    /// \return 0 on success, -1 on failure.
    int try_lock();

    /// Override to implement keep_quiet_ in a sane way
    int logf(log_level_t level, const char *fmt, ...) PRINTFLIKE(3, 4);

protected:
    pthread_mutex_t mutex_;
    lock_type_t     type_;
    bool	    keep_quiet_;
};

#endif /* _MUTEX_H_ */


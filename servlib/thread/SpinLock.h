#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include "debug/Debug.h"
#include "Atomic.h"
#include "Lock.h"
#include "Thread.h"

/**
 * If we know there are no atomic instructions for the architecture,
 * just use a Mutex.
 */
#ifdef __NO_ATOMIC__
#include "Mutex.h"
class SpinLock : public Mutex {
public:
    SpinLock() : Mutex("spinlock", TYPE_RECURSIVE, true) {}
};
#else

/**
 * A SpinLock is a Lock that busy waits to get a lock. The
 * implementation supports recursive locking.
 */
class SpinLock : public Lock {
public:

public:
    SpinLock() : Lock(), lock_count_(0) {}
    virtual ~SpinLock() {}

    /// @{
    /// Virtual override from Lock
    inline int lock();
    inline int unlock();
    inline int try_lock();
    /// @}
    
private:
    unsigned int lock_count_; ///< count for recursive locking
};

int
SpinLock::lock()
{
    pthread_t me = Thread::current();
    int nspins;
    
    if (lock_holder_ == me) {
        lock_count_++;
        return 0;
    }
    
    nspins = 0;
    while (atomic_cmpxchg32(&lock_holder_, 0, (unsigned int)me) != 0)
    {
        Thread::spin_yield();
#ifndef NDEBUG
        if (++nspins > 100000000) {
            PANIC("SpinLock reached spin limit");
        }
#endif
    }

    ASSERT(lock_count_ == 0);
    lock_count_ = 1;
    return 0;
};

int
SpinLock::unlock() {
    ASSERT(lock_holder_ == Thread::current());
    ASSERT(lock_count_ > 0);

    lock_count_--;
    if (lock_count_ == 0) {
        lock_holder_ = 0;
    }
    
    return 0;
};
 
int
SpinLock::try_lock()
{
    pthread_t me = Thread::current();
    int lock_holder = atomic_cmpxchg32(&lock_holder_, 0, (unsigned int)me);

    if (lock_holder == 0) {
        ASSERT(lock_count_ == 0);
        lock_count_++;
        return 0; // success
    } else {
        return 1; // already locked
    }
};

#endif /* __NO_ATOMIC__ */

#endif /* _SPINLOCK_H_ */

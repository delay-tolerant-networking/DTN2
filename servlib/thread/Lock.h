#ifndef _LOCK_H_
#define _LOCK_H_

#include "debug/Debug.h"
#include "Thread.h"

/**
 * Abstract Lock base class.
 */
class Lock {
public:
    /**
     * Default Lock constructor.
     */
    Lock() : lock_holder_(0), scope_lock_count_(0) {}

    /**
     * Lock destructor. Asserts that the lock is not locked by another
     * thread at the time of destruction.
     */
    virtual ~Lock()
    {
        ASSERT(!is_locked() ||
               (is_locked_by_me() && scope_lock_count_ == 0));
    }
    
    /**
     * Acquire the lock.
     *
     * @return 0 on success, -1 on error
     */
    virtual int lock() = 0;

    /**
     * Release the lock.
     *
     * @return 0 on success, -1 on error
     */
    virtual int unlock() = 0;

    /**
     * Try to acquire the lock.
     *
     * @return 0 on success, 1 if already locked, -1 on error.
     */
    virtual int try_lock() = 0;

    /**
     * Check for whether the lock is locked or not.
     *
     * @return true if locked, false otherwise
     */
    bool is_locked()
    {
        return (lock_holder_ != 0);
    }

    /**
     * Check for whether the lock is locked or not by the calling
     * thread.
     *
     * @return true if locked by the current thread, false otherwise
     */
    bool is_locked_by_me()
    {
        return (lock_holder_ == Thread::current());
    }

protected:
    friend class ScopeLock;
    
    /**
     * Stores the thread id of the current lock holder. It is the
     * responsibility of the derived class to set this in lock() and
     * unset it in unlock(), since the accessors is_locked() and
     * is_locked_by_me() depend on it.
     */
    pthread_t lock_holder_;

    /**
     * Stores a count of the number of ScopeLocks holding the lock.
     * This is checked in the Lock destructor to avoid strange crashes
     * if you delete the lock object and then the ScopeLock destructor
     * tries to unlock it.
     */
    int scope_lock_count_;
     
};

/**
 * Scope based lock created from a Lock. Holds the lock until the
 * object is destructed. Example of use:
 *
 * \code
 * {
 *     Mutex m;
 *     ScopeLock lock(&m);
 *     // protected code
 *     ...
 * }
 * \endcode
 */
class ScopeLock {
public:
    ScopeLock(Lock* l) : lock_(l)
    {
        int ret = lock_->lock();
        ASSERT(ret == 0);
        lock_->scope_lock_count_++;
    }
    
    void unlock() {
        lock_->scope_lock_count_--;
        lock_->unlock();
        lock_ = NULL;
    }
    
    ~ScopeLock()
    {
        if (lock_) {
            unlock();
        }
    }
    
protected:
    Lock* lock_;
};

#endif /* LOCK_h */

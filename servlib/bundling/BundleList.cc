
#include "BundleList.h"
#include "thread/SpinLock.h"

BundleList::BundleList()
    : lock_(new SpinLock()), waiter_(false)
{
}

BundleList::~BundleList()
{
    delete lock_;
}

Bundle*
BundleList::front()
{
    ScopeLock l(lock_);
    return list_.front();
}

Bundle*
BundleList::back()
{
    ScopeLock l(lock_);
    return list_.back();
}

Bundle*
BundleList::pop_front()
{
    ScopeLock l(lock_);
    Bundle* ret = list_.front();
    list_.pop_front();
    return ret;
}

Bundle*
BundleList::pop_back()
{
    ScopeLock l(lock_);
    Bundle* ret = list_.back();
    list_.pop_back();
    return ret;
}

Bundle*
BundleList::pop_blocking()
{
    lock_->lock();

    if (size() == 0) {
        if (waiter_) {
            PANIC("BundleList doesn't support multiple waiting threads");
        }
        waiter_ = true;
        
        lock_->unlock();
        wait();
        lock_->lock();
        
        waiter_ = false;
    }

    // always drain the pipe when done waiting
    drain_pipe();
    
    ASSERT(size() > 0);
    Bundle* ret = list_.front();
    ASSERT(ret != NULL);
    list_.pop_front();
    
    lock_->unlock();
    
    return ret;
}

void
BundleList::push_front(Bundle* b)
{
    ScopeLock l(lock_);
    list_.push_front(b);

    if (waiter_ == true && size() == 1)
    {
        notify();
    }
}

void
BundleList::push_back(Bundle* b)
{
    ScopeLock l(lock_);
    list_.push_back(b);
    
    if (waiter_ == true && size() == 1)
    {
        notify();
    }
}

size_t
BundleList::size()
{
    ScopeLock l(lock_);
    return list_.size();
}

BundleList::iterator
BundleList::begin()
{
    ASSERT(lock_->is_locked_by_me());
    return list_.begin();
}

BundleList::iterator
BundleList::end()
{
    ASSERT(lock_->is_locked_by_me());
    return list_.end();
}

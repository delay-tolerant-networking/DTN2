
#include "Bundle.h"
#include "BundleList.h"
#include "thread/SpinLock.h"

BundleList::BundleList(const std::string& name)
    : lock_(new SpinLock()), name_(name)
{
}

BundleList::~BundleList()
{
    delete lock_;
}

/**
 * Peek at the first bundle on the list.
 */
Bundle*
BundleList::front()
{
    ScopeLock l(lock_);

    if (list_.empty())
        return NULL;
    
    return list_.front();
}

/**
 * Peek at the last bundle on the list.
 */
Bundle*
BundleList::back()
{
    ScopeLock l(lock_);

    if (list_.empty())
        return NULL;
    
    return list_.back();
}

/**
 * Add a new bundle to the front of the list.
 */
void
BundleList::push_front(Bundle* b)
{
    ScopeLock l(lock_);
    list_.push_front(b);

    b->add_ref();
    bool added = b->add_container(this);
    ASSERT(added);

    if (size() == 1)
    {
        notify();
    }
}

/**
 * Add a new bundle to the front of the list.
 */
void
BundleList::push_back(Bundle* b)
{
    ScopeLock l(lock_);
    list_.push_back(b);

    b->add_ref();
    bool added = b->add_container(this);
    ASSERT(added);
    
    if (size() == 1)
    {
        notify();
    }
}

/**
 * Remove (and return) the first bundle on the list.
 */
Bundle*
BundleList::pop_front()
{
    ScopeLock l(lock_);

    // always drain the pipe to make sure that if the list is empty,
    // then the pipe must be empty as well
    drain_pipe();
    
    if (list_.empty())
        return NULL;

    Bundle* b = list_.front();
    list_.pop_front();

    bool deleted = b->del_container(this);
    ASSERT(deleted);
        
    return b;
}

/**
 * Remove (and return) the last bundle on the list.
 */
Bundle*
BundleList::pop_back()
{
    ScopeLock l(lock_);

    // always drain the pipe to maintain the invariant that if the
    // list is empty, then the pipe must be empty as well
    drain_pipe();
    
    if (list_.empty())
        return NULL;
    
    Bundle* b = list_.back();
    list_.pop_back();

    bool deleted = b->del_container(this);
    ASSERT(deleted);
        
    return b;
}

/**
 * Remove (and return) the first bundle on the list, blocking if
 * there are none.
 */
Bundle*
BundleList::pop_blocking(int timeout)
{
    lock_->lock();

    if (list_.empty()) {
        bool notified = wait(lock_, timeout);
        ASSERT(lock_->is_locked_by_me());

        // if the timeout occurred, wait returns false, so there's
        // still nothing on the list
        if (!notified) {
            lock_->unlock();
            return NULL;
        }

        // the pipe is drained in pop_front()
    }

    ASSERT(!list_.empty());
    
    Bundle* b = pop_front();
    
    lock_->unlock();
    
    return b;
}

/**
 * Remove the given bundle.
 *
 * @return true if the bundle was removed successfully, false if
 * it was not on the list
 */
bool
BundleList::remove(Bundle* bundle)
{
    ScopeLock l(lock_);

    iterator iter = std::find(list_.begin(), list_.end(), bundle);
    if (iter == list_.end()) {
        return false;
    }

    list_.erase(iter);

    bool deleted = bundle->del_container(this);
    ASSERT(deleted);
    
    bundle->del_ref(); // may delete the bundle

    return true;
}

/**
 * Return the size of the list.
 */
size_t
BundleList::size()
{
    ScopeLock l(lock_);
    return list_.size();
}

/**
 * Iterator used to iterate through the list. Iterations _must_ be
 * completed while holding the list lock, and this method will
 * assert as such.
 */
BundleList::iterator
BundleList::begin()
{
    ASSERT(lock_->is_locked_by_me());
    return list_.begin();
}

/**
 * Iterator used to mark the end of the list. Iterations _must_ be
 * completed while holding the list lock, and this method will
 * assert as such.
 */
BundleList::iterator
BundleList::end()
{
    ASSERT(lock_->is_locked_by_me());
    return list_.end();
}


#include "BundleList.h"
#include "thread/SpinLock.h"

BundleList::BundleList()
    : lock_(new SpinLock())
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

void
BundleList::push_front(Bundle* b)
{
    ScopeLock l(lock_);
    list_.push_front(b);
}

void
BundleList::push_back(Bundle* b)
{
    ScopeLock l(lock_);
    list_.push_back(b);
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

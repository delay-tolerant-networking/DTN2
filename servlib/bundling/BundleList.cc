
#include "Bundle.h"
#include "BundleList.h"
#include "thread/SpinLock.h"
#include <algorithm>

// XXX/demmer want some sort of expiration handler registration per
// list so things know when their bundles have been expired

BundleList::BundleList(const std::string& name)
    : lock_(new SpinLock()), name_(name)
{
}

BundleList::~BundleList()
{
    clear();
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
 * Helper routine to do bookkeeping when a bundle is added.
 */
void
BundleList::add_bundle(Bundle* b)
{
    b->add_ref("bundle_list", name_.c_str());
    bool added = b->add_container(this);
    ASSERT(added);
}


/**
 * Add a new bundle to the front of the list.
 */
void
BundleList::push_front(Bundle* b)
{
    lock_->lock();
    list_.push_front(b);
    add_bundle(b);
    lock_->unlock();
    
    if (size() == 1) {
        notify();
    }
}

/**
 * Add a new bundle to the front of the list.
 */
void
BundleList::push_back(Bundle* b)
{
    lock_->lock();
    list_.push_back(b);
    add_bundle(b);
    lock_->unlock();
    
    if (size() == 1) {
        notify();
    }
}
        
/**
 * Insert the given bundle sorted by the given sort method.
 */
void
BundleList::insert_sorted(Bundle* b, sort_order_t sort_order)
{
    ListType::iterator iter;

    lock_->lock();

    // scan through the list until the iterator either a) reaches the
    // end of the list or b) reaches the bundle that should follow the
    // new insertion in the list. once the loop is done therefore, the
    // insert() call will then always put the bundle in the right
    // place
    //
    // XXX/demmer there's probably a more stl-ish way to do this but i
    // don't know what it is 
    
    for (iter = list_.begin(); iter != list_.end(); ++iter)
    {
        if (sort_order == SORT_FRAG_OFFSET) {
            if ((*iter)->frag_offset_ > b->frag_offset_) {
                break;
            }

        } else if (sort_order == SORT_PRIORITY) {
            NOTIMPLEMENTED;
            
        } else {
            PANIC("invalid value for sort order %d", sort_order);
        }
    }

    list_.insert(iter, b);
    add_bundle(b);
    lock_->unlock();
    
    if (size() == 1) {
        notify();
    }
}

/**
 * Helper routine to do bookkeeping when a bundle is removed.
 */
void
BundleList::del_bundle(Bundle* b)
{
    bool deleted = b->del_container(this);
    ASSERT(deleted);
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

    del_bundle(b);
    
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

    del_bundle(b);
        
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
    
    bundle->del_ref("bundle_list", name_.c_str()); // may delete the bundle

    return true;
}

/**
 * Move all bundles from this list to another.
 */
void
BundleList::move_contents(BundleList* other)
{
    ScopeLock l1(lock_);
    ScopeLock l2(other->lock_);

    Bundle* b;
    while (!list_.empty()) {
        b = pop_front();
        other->push_back(b);
        b->del_ref("BundleList move_contents from", name_.c_str());
    }
}

/**
 * Clear out the list.
 */
void
BundleList::clear()
{
    Bundle* b;
    ScopeLock l(lock_);
    
    while (!list_.empty()) {
        b = pop_front();
        b->del_ref("bundle_list", name_.c_str()); // may delete the bundle
    }
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
    if (!lock_->is_locked_by_me())
        PANIC("Must lock BundleList before using iterator");
    
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
    if (!lock_->is_locked_by_me())
        PANIC("Must lock BundleList before using iterator");
    
    return list_.end();
}

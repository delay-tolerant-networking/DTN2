/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <algorithm>
#include <oasys/thread/SpinLock.h>

#include "Bundle.h"
#include "BundleList.h"

namespace dtn {

// XXX/demmer want some sort of expiration handler registration per
// list so things know when their bundles have been expired

// XXX/demmer don't always create the notifier to save on open file
// descriptors... we only really need it for the contact

BundleList::BundleList(const std::string& name)
    : name_(name), lock_(new oasys::SpinLock()), notifier_(NULL)
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
BundleRef
BundleList::front()
{
    oasys::ScopeLock l(lock_, "BundleList::front");
    
    BundleRef ret("BundleList::front() temporary");
    if (list_.empty())
        return ret;

    ret = list_.front();
    return ret;
}

/**
 * Peek at the last bundle on the list.
 */
BundleRef
BundleList::back()
{
    oasys::ScopeLock l(lock_, "BundleList::back");

    BundleRef ret("BundleList::back() temporary");
    if (list_.empty())
        return ret;

    ret = list_.back();
    return ret;
}

/**
 * Helper routine to insert a bundle immediately before the indicated
 * position.
 */
void
BundleList::add_bundle(Bundle* b, const iterator& pos)
{
    ASSERT(lock_->is_locked_by_me());
    ASSERT(b->lock_.is_locked_by_me());

    if (b->mappings_.insert(this).second == false) {
        log_err("/bundle/mapping", "ERROR in add mapping: "
                "bundle id %d already on list [%s]",
                b->bundleid_, name_.c_str());

        return;
    }

    list_.insert(pos, b);
    b->add_ref("bundle_list", name_.c_str()); 
    if (notifier_ != 0) {
        notifier_->notify();
    }

    log_debug("/bundle/mapping", "bundle id %d add mapping [%s] to list %p",
              b->bundleid_, name_.c_str(), this);
}

/**
 * Add a new bundle to the front of the list.
 */
void
BundleList::push_front(Bundle* b)
{
    oasys::ScopeLock l(lock_, "BundleList::push_front");
    oasys::ScopeLock bl(&b->lock_, "BundleList::push_front");
    add_bundle(b, list_.begin());
}

/**
 * Add a new bundle to the front of the list.
 */
void
BundleList::push_back(Bundle* b)
{
    oasys::ScopeLock l(lock_, "BundleList::push_back");
    oasys::ScopeLock bl(&b->lock_, "BundleList::push_back");
    add_bundle(b, list_.end());
}
        
/**
 * Insert the given bundle sorted by the given sort method.
 */
void
BundleList::insert_sorted(Bundle* b, sort_order_t sort_order)
{
    iterator iter;
    oasys::ScopeLock l(lock_, "BundleList::insert_sorted");
    oasys::ScopeLock bl(&b->lock_, "BundleList::insert_sorted");

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
    
    add_bundle(b, iter);
}

/**
 * Helper routine to remove a bundle from the indicated position.
 */
Bundle*
BundleList::del_bundle(const iterator& pos, bool used_notifier)
{
    Bundle* b = *pos;
    ASSERT(lock_->is_locked_by_me());
    
    // lock the bundle
    oasys::ScopeLock l(& b->lock_, "BundleList::del_bundle");

    // remove the mapping
    log_debug("/bundle/mapping",
              "bundle id %d del_bundle: deleting mapping [%s]",
              b->bundleid_, name_.c_str());
    
    Bundle::BundleMappings::iterator mapping_pos = b->mappings_.find(this);
    if (mapping_pos == b->mappings_.end()) {
        log_err("/bundle/mapping", "ERROR in del mapping: "
                "bundle id %d not on list [%s]",
                b->bundleid_, name_.c_str());
    } else {
        b->mappings_.erase(mapping_pos);
    }

    // remove the bundle from the list
    list_.erase(pos);
    
    // drain one element from the semaphore
    if (notifier_ && !used_notifier) {
        notifier_->drain_pipe(1);
    }

    // note that we explicitly do _not_ decrement the reference count
    // since the reference is passed to the calling function
    
    return b;
}

/**
 * Remove (and return) the first bundle on the list.
 */
BundleRef
BundleList::pop_front(bool used_notifier)
{
    oasys::ScopeLock l(lock_, "pop_front");

    BundleRef ret("BundleList::pop_front() temporary");

    if (list_.empty()) {
        return ret;
    }
    
    ASSERT(list_.size() != 0);

    // Assign the bundle to a temporary reference, then remove the
    // list reference on the bundle and return the temporary
    ret = del_bundle(list_.begin(), used_notifier);
    ret.object()->del_ref("bundle_list", name_.c_str()); 
    return ret;
}

/**
 * Remove (and return) the last bundle on the list.
 */
BundleRef
BundleList::pop_back(bool used_notifier)
{
    oasys::ScopeLock l(lock_, "BundleList::pop_back");

    BundleRef ret("BundleList::pop_back() temporary");

    if (list_.empty()) {
        return ret;
    }

    // Assign the bundle to a temporary reference, then remove the
    // list reference on the bundle and return the temporary
    ret = del_bundle(--list_.end(), used_notifier);
    ret->del_ref("bundle_list", name_.c_str()); 
    return ret;
}

/**
 * Remove the given bundle from the list. Returns true if the
 * bundle was successfully removed, false otherwise.
 *
 * Unlike the pop() functions, this does remove the list's
 * reference on the bundle.
 */
bool
BundleList::erase(Bundle* bundle, bool used_notifier)
{
    if (bundle == NULL) {
        return false;
    }

    ASSERTF(!bundle->lock_.is_locked_by_me(),
            "bundle cannot be locked in erase due to potential deadlock");
    
    oasys::ScopeLock l(lock_, "BundleList::erase");

    iterator pos = std::find(begin(), end(), bundle);
    if (pos == end()) {
        return false;
    }

    del_bundle(pos, used_notifier);
    
    bundle->del_ref("bundle_list", name_.c_str());
    return true;
}

/**
 * Search the list for the given bundle.
 *
 * @return true if found, false if not
 */
bool
BundleList::contains(Bundle* bundle)
{
    if (bundle == NULL) {
        return false;
    }
    
    bool ret = bundle->is_queued_on(this);

#define DEBUG_MAPPINGS
#ifdef DEBUG_MAPPINGS
    oasys::ScopeLock l(lock_, "BundleList::contains");
    bool ret2 = (std::find(begin(), end(), bundle) != end());
    ASSERT(ret == ret2);
#endif

    return ret;
}

/**
 * Search the list for a bundle with the given id.
 *
 * @return the bundle or NULL if not found.
 */
BundleRef
BundleList::find(u_int32_t bundle_id)
{
    oasys::ScopeLock l(lock_, "BundleList::find");
    BundleRef ret("BundleList::find() temporary");
    for (iterator iter = begin(); iter != end(); ++iter) {
        if ((*iter)->bundleid_ == bundle_id) {
            ret = *iter;
            return ret;
        }
    }

    return ret;
}

/**
 * Search the list for a bundle with the given source eid and
 * timestamp.
 *
 * @return the bundle or NULL if not found.
 */
BundleRef
BundleList::find(const EndpointID& source_eid,
                 const struct timeval& creation_ts)
{
    oasys::ScopeLock l(lock_, "BundleList::find");
    BundleRef ret("BundleList::find() temporary");
    
    for (iterator iter = begin(); iter != end(); ++iter) {
        if ((*iter)->source_.equals(source_eid) &&
            (*iter)->creation_ts_.tv_sec == creation_ts.tv_sec &&
            (*iter)->creation_ts_.tv_usec == creation_ts.tv_usec)
        {
            ret = *iter;
            return ret;
        }
    }

    return ret;
}

/**
 * Move all bundles from this list to another.
 */
void
BundleList::move_contents(BundleList* other)
{
    oasys::ScopeLock l1(lock_, "BundleList::move_contents");
    oasys::ScopeLock l2(other->lock_, "BundleList::move_contents");

    BundleRef b("BundleList::move_contents temporary");
    while (!list_.empty()) {
        b = pop_front();
        other->push_back(b.object());
    }
}

/**
 * Clear out the list.
 */
void
BundleList::clear()
{
    oasys::ScopeLock l(lock_, "BundleList::clear");
    
    while (!list_.empty()) {
        BundleRef b("BundleList::clear temporary");
        b = pop_front();
    }
}


/**
 * Return the size of the list.
 */
size_t
BundleList::size() const
{
    oasys::ScopeLock l(lock_, "BundleList::size");
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

/**
 * Const iterator used to iterate through the list. Iterations 
 * _must_ be completed while holding the list lock, and this 
 * method will assert as such.
 */
BundleList::const_iterator
BundleList::begin() const
{
    if (!lock_->is_locked_by_me())
        PANIC("Must lock BundleList before using iterator");
    
    return list_.begin();
}


/**
 * Const iterator used to mark the end of the list. Iterations
 * _must_ be completed while holding the list lock, and this
 * method will assert as such.
 */
BundleList::const_iterator
BundleList::end() const
{
    if (!lock_->is_locked_by_me())
        PANIC("Must lock BundleList before using iterator");
    
    return list_.end();
}

BlockingBundleList::BlockingBundleList(const std::string& name)
    : BundleList(name)
{
    if (name[0] == '/') {
        notifier_ = new oasys::Notifier(name.c_str());
    } else {
        notifier_ = new oasys::Notifier("/bundle/list");
    }
}

BlockingBundleList::~BlockingBundleList()
{
    delete notifier_;
    notifier_ = NULL;
}

/**
 * Remove (and return) the first bundle on the list, blocking if
 * there are none. 
 *
 * Blocking read can be interrupted by calling BundleList::notify(). 
 * If this is done, the return value will be NULL.
 */
BundleRef
BlockingBundleList::pop_blocking(int timeout)
{
    ASSERT(notifier_);

    log_debug("/bundle/mapping", "pop_blocking on list %p [%s]", 
              this, name_.c_str());
    
    lock_->lock("BlockingBundleList::pop_blocking");

    bool used_wait;
    if (list_.empty()) {
        used_wait = true;
        bool notified = notifier_->wait(lock_, timeout);
        ASSERT(lock_->is_locked_by_me());

        // if the timeout occurred, wait returns false, so there's
        // still nothing on the list
        if (!notified) {
            lock_->unlock();
            log_debug("/bundle/mapping",
                      "pop_blocking timeout on list %p", this);

            return 0;
        }
    } else {
        used_wait = false;
    }
    
    // This can't be empty if we got notified, unless there is another
    // thread waiting on the queue - which is an error.
    ASSERT(!list_.empty());
    
    BundleRef ret("BlockingBundleList::pop_blocking temporary");
    ret = pop_front(used_wait);
    
    lock_->unlock();

    log_debug("/bundle/mapping", "pop_blocking got bundle %p from list %p", 
              ret.object(), this);
    
    return ret;
}


} // namespace dtn

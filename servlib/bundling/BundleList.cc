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
#include "BundleMapping.h"

namespace dtn {

// XXX/demmer want some sort of expiration handler registration per
// list so things know when their bundles have been expired

// XXX/demmer don't always create the notifier to save on open file
// descriptors... we only really need it for the contact

BundleList::BundleList(const std::string& name)
    : lock_(new oasys::SpinLock()), name_(name)
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
    oasys::ScopeLock l(lock_);

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
    oasys::ScopeLock l(lock_);

    if (list_.empty())
        return NULL;
    
    return list_.back();
}

/**
 * Helper routine to insert a bundle immediately before the indicated
 * position.
 */
void
BundleList::add_bundle(Bundle* b, iterator pos,
                       const BundleMapping* mapping_info)
{
    ASSERT(lock_->is_locked_by_me());

    list_.insert(pos, b);
    
    b->add_ref("bundle_list", name_.c_str());

    BundleMapping* mapping = b->add_mapping(this, mapping_info);
    ASSERT(mapping);

    mapping->position_ = --pos;
    ASSERT(*(mapping->position_) == b);
}


/**
 * Add a new bundle to the front of the list.
 */
void
BundleList::push_front(Bundle* b, const BundleMapping* mapping_info)
{
    lock_->lock();
    add_bundle(b, list_.begin(), mapping_info);
    lock_->unlock();
    
    if (size() == 1) {
        notify();
    }
}

/**
 * Add a new bundle to the front of the list.
 */
void
BundleList::push_back(Bundle* b, const BundleMapping* mapping_info)
{
    lock_->lock();
    add_bundle(b, list_.end(), mapping_info);
    lock_->unlock();
    
    if (size() == 1) {
        notify();
    }
}
        
/**
 * Insert the given bundle sorted by the given sort method.
 */
void
BundleList::insert_sorted(Bundle* b, const BundleMapping* mapping_info,
                          sort_order_t sort_order)
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
    
    add_bundle(b, iter, mapping_info);
    lock_->unlock();
    
    if (size() == 1) {
        notify();
    }
}

/**
 * Helper routine to remove a bundle from the indicated position.
 */
Bundle*
BundleList::del_bundle(iterator pos, BundleMapping** mappingp)
{
    Bundle* b = *pos;
    ASSERT(lock_->is_locked_by_me());
    
    BundleMapping* mapping = b->del_mapping(this);
    ASSERT(mapping->position_ == pos);
    ASSERT(mapping != NULL);
    
    list_.erase(pos);

    // note that we explicitly do _not_ decrement the reference count
    // since the reference is passed to the calling function
    
    // if the caller wants the mapping, return it, otherwise delete it
    if (mappingp) {
        *mappingp = mapping;
    } else {
        delete mapping;
    }

    return b;
}

/**
 * Remove (and return) the first bundle on the list.
 */
Bundle*
BundleList::pop_front(BundleMapping** mappingp)
{
    oasys::ScopeLock l(lock_);

    // always drain the pipe to make sure that if the list is empty,
    // then the pipe must be empty as well
    drain_pipe();
    
    if (list_.empty())
        return NULL;

    return del_bundle(list_.begin(), mappingp);
}

/**
 * Remove (and return) the last bundle on the list.
 */
Bundle*
BundleList::pop_back(BundleMapping** mappingp)
{
    oasys::ScopeLock l(lock_);

    // always drain the pipe to maintain the invariant that if the
    // list is empty, then the pipe must be empty as well
    drain_pipe();

    if (list_.empty())
        return NULL;

    return del_bundle(--list_.end(), mappingp);
}

/**
 * Remove (and return) the first bundle on the list, blocking if
 * there are none.
 */
Bundle*
BundleList::pop_blocking(BundleMapping** mappingp, int timeout)
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
    
    Bundle* b = pop_front(mappingp);
    
    lock_->unlock();
    
    return b;
}

/**
 * Remove the bundle at the given list position. Always succeeds
 * since the iterator must be valid.
 *
 * Unlike the pop() functions, this does remove the list's
 * reference on the bundle.
 */
void
BundleList::erase(iterator& pos, BundleMapping** mappingp)
{
    oasys::ScopeLock l(lock_);
    
    Bundle* b = del_bundle(pos, mappingp);
    b->del_ref("bundle_list", name_.c_str());
}

/**
 * Move all bundles from this list to another.
 */
void
BundleList::move_contents(BundleList* other)
{
    oasys::ScopeLock l1(lock_);
    oasys::ScopeLock l2(other->lock_);

    Bundle* b;
    BundleMapping* m;
    while (!list_.empty()) {
        b = pop_front(&m);
        other->push_back(b, m); // copies m
        delete m;
        
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
    oasys::ScopeLock l(lock_);
    
    while (!list_.empty()) {
        b = pop_front();
        b->del_ref("bundle_list", name_.c_str()); // may delete the bundle
    }
}


/**
 * Return the size of the list.
 */
size_t
BundleList::size() const
{
    oasys::ScopeLock l(lock_);
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

} // namespace dtn

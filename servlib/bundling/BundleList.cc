/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <algorithm>
#include <stdlib.h>
#include <oasys/thread/SpinLock.h>

#include "Bundle.h"
#include "BundleList.h"
#include "BundleMappings.h"
#include "BundleTimestamp.h"
#include "BundleDaemon.h"

namespace dtn {

//----------------------------------------------------------------------
BundleList::BundleList(const std::string& name, oasys::SpinLock* lock)
    : Logger("BundleList", "/dtn/bundle/list/%s", name.c_str()),
      name_(name), notifier_(NULL)
{
    if (lock != NULL) {
        lock_     = lock;
        own_lock_ = false;
    } else {
        lock_     = new oasys::SpinLock();
        own_lock_ = true;
    }
}

//----------------------------------------------------------------------
void
BundleList::set_name(const std::string& name)
{
    name_ = name;
    logpathf("/dtn/bundle/list/%s", name.c_str());
}

//----------------------------------------------------------------------
BundleList::~BundleList()
{
    clear();
    if (own_lock_) {
        delete lock_;
    }
    lock_ = NULL;
}

//----------------------------------------------------------------------
BundleRef
BundleList::front() const
{
    oasys::ScopeLock l(lock_, "BundleList::front");
    
    BundleRef ret("BundleList::front() temporary");
    if (list_.empty())
        return ret;

    ret = list_.front();
    return ret;
}

//----------------------------------------------------------------------
BundleRef
BundleList::back() const
{
    oasys::ScopeLock l(lock_, "BundleList::back");

    BundleRef ret("BundleList::back() temporary");
    if (list_.empty())
        return ret;

    ret = list_.back();
    return ret;
}

//----------------------------------------------------------------------
void
BundleList::add_bundle(Bundle* b, const iterator& pos)
{
	int ret;

    ASSERT(lock_->is_locked_by_me());
    ASSERT(b->lock()->is_locked_by_me());
    
    if (b->is_freed()) {
    	log_info("add_bundle called with pre-freed bundle; ignoring");
    	return;
    }

    if (b->is_queued_on(this)) {
        log_err("ERROR in add bundle: "
                "bundle id %d already on list [%s]",
                b->bundleid(), name_.c_str());
        
        return;
    }
    
    iterator new_pos = list_.insert(pos, b);
    b->mappings()->push_back(BundleMapping(this, new_pos));
    b->add_ref("bundle_list", name_.c_str());
    
	if (notifier_ != 0) {
		notifier_->notify();
	}

    log_debug("bundle id %d add mapping [%s] to list %p",
              b->bundleid(), name_.c_str(), this);
}

//----------------------------------------------------------------------
void
BundleList::push_front(Bundle* b)
{
    oasys::ScopeLock l(lock_, "BundleList::push_front");
    oasys::ScopeLock bl(b->lock(), "BundleList::push_front");
    add_bundle(b, list_.begin());
}

//----------------------------------------------------------------------
void
BundleList::push_back(Bundle* b)
{
    oasys::ScopeLock l(lock_, "BundleList::push_back");
    oasys::ScopeLock bl(b->lock(), "BundleList::push_back");
    add_bundle(b, list_.end());
}
        
//----------------------------------------------------------------------
void
BundleList::insert_sorted(Bundle* b, sort_order_t sort_order)
{
    iterator iter;
    oasys::ScopeLock l(lock_, "BundleList::insert_sorted");
    oasys::ScopeLock bl(b->lock(), "BundleList::insert_sorted");

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
            if ((*iter)->frag_offset() > b->frag_offset()) {
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

//----------------------------------------------------------------------
void
BundleList::insert_random(Bundle* b)
{
    iterator iter;
    oasys::ScopeLock l(lock_, "BundleList::insert_random");
    oasys::ScopeLock bl(b->lock(), "BundleList::insert_random");

    iter = begin();
    int location = 0;
    if (! empty()) {
        location = random() % size();
    }

    log_info("insert_random at %d/%zu", location, size());
    
    for (int i = 0; i < location; ++i) {
        ++iter;
    }

    add_bundle(b, iter);
}

//----------------------------------------------------------------------
Bundle*
BundleList::del_bundle(const iterator& pos, bool used_notifier)
{
    Bundle* b = *pos;
    ASSERT(lock_->is_locked_by_me());
    
    // lock the bundle
    oasys::ScopeLock l(b->lock(), "BundleList::del_bundle");

    // remove the mapping
    log_debug("bundle id %d del_bundle: deleting mapping [%s]",
              b->bundleid(), name_.c_str());
    BundleMappings::iterator mapping = b->mappings()->find(this);
    if (mapping == b->mappings()->end()) {
        log_err("ERROR in del bundle: "
                "bundle id %d has no mapping for list [%s]",
                b->bundleid(), name_.c_str());
    } else {
        ASSERT(mapping->list() == this);
        ASSERT(mapping->position() == pos);
        b->mappings()->erase(mapping);
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

//----------------------------------------------------------------------
BundleRef
BundleList::pop_front(bool used_notifier)
{
    oasys::ScopeLock l(lock_, "pop_front");

    BundleRef ret("BundleList::pop_front() temporary");

    if (list_.empty()) {
        return ret;
    }
    
    ASSERT(!empty());

    // Assign the bundle to a temporary reference, then remove the
    // list reference on the bundle and return the temporary
    ret = del_bundle(list_.begin(), used_notifier);
    ret.object()->del_ref("bundle_list", name_.c_str()); 
    return ret;
}

//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
bool
BundleList::erase(Bundle* bundle, bool used_notifier)
{
    if (bundle == NULL) {
        return false;
    }

    // The bundle list lock must always be taken before the
    // to-be-erased bundle lock.
    ASSERTF(!bundle->lock()->is_locked_by_me(),
            "bundle cannot be locked before calling erase "
            "due to potential deadlock");
    
    oasys::ScopeLock l(lock_, "BundleList::erase");

    // Now we need to take the bundle lock in order to search through
    // its mappings
    oasys::ScopeLock bl(bundle->lock(), "BundleList::erase");
    
    BundleMappings::iterator mapping = bundle->mappings()->find(this);
    if (mapping == bundle->mappings()->end()) {
        return false;
    }

    ASSERT(mapping->list() == this);
    ASSERT(*mapping->position() == bundle);

    // Make a local copy of the position since del_bundle destroys the
    // mapping object but still needs the position.
    iterator pos = mapping->position();
    Bundle* b = del_bundle(pos, used_notifier);
    ASSERT(b == bundle);
    
    bundle->del_ref("bundle_list", name_.c_str());
    return true;
}

//----------------------------------------------------------------------
void
BundleList::erase(iterator& iter, bool used_notifier)
{
    Bundle* bundle = *iter;
    ASSERTF(!bundle->lock()->is_locked_by_me(),
            "bundle cannot be locked in erase due to potential deadlock");
    
    oasys::ScopeLock l(lock_, "BundleList::erase");
    
    Bundle* b = del_bundle(iter, used_notifier);
    ASSERT(b == bundle);
    
    bundle->del_ref("bundle_list", name_.c_str());
}

//----------------------------------------------------------------------
bool
BundleList::contains(Bundle* bundle) const
{
    oasys::ScopeLock l(lock_, "BundleList::contains");
    
    if (bundle == NULL) {
        return false;
    }
    
    bool ret = bundle->is_queued_on(this);

#define DEBUG_MAPPINGS
#ifdef DEBUG_MAPPINGS
    bool ret2 = (std::find(begin(), end(), bundle) != end());
    ASSERT(ret == ret2);
#endif

    return ret;
}

//----------------------------------------------------------------------
BundleRef
BundleList::find(u_int32_t bundle_id) const
{
    oasys::ScopeLock l(lock_, "BundleList::find");
    BundleRef ret("BundleList::find() temporary (by bundle_id)");

    for (iterator iter = begin(); iter != end(); ++iter) {
    	// We need to exclude freed bundles here, otherwise the refcounting
    	// gets really hosed up, as a find after a bundle is freed can cause
    	// an extra reference to it.  Trying to simply not add refs to freed
    	// bundles causes other problems when those refs are deleted, leading
    	// to too low a refcount in the bundle.
        if ((*iter)->bundleid() == bundle_id && !((*iter)->is_freed())) {
            ret = *iter;
            return ret;
        }
    }

    return ret;
}

//----------------------------------------------------------------------
BundleRef
BundleList::find(const EndpointID& source_eid,
                 const BundleTimestamp& creation_ts) const
{
    oasys::ScopeLock l(lock_, "BundleList::find");
    BundleRef ret("BundleList::find() temporary");
    
    for (iterator iter = begin(); iter != end(); ++iter) {
        if ((*iter)->creation_ts().seconds_ == creation_ts.seconds_ &&
            (*iter)->creation_ts().seqno_ == creation_ts.seqno_ &&
            (*iter)->source().equals(source_eid) &&
            !((*iter)->is_freed()))
        {
            ret = *iter;
            return ret;
        }
    }

    return ret;
}

//----------------------------------------------------------------------
BundleRef
BundleList::find(GbofId& gbof_id) const
{
    oasys::ScopeLock l(lock_, "BundleList::find");
    BundleRef ret("BundleList::find() temporary (by gbof_id)");
    
    for (iterator iter = begin(); iter != end(); ++iter) {
        if (gbof_id.equals((*iter)->source(),
                           (*iter)->creation_ts(),
                           (*iter)->is_fragment(),
                           (*iter)->payload().length(),
                           (*iter)->frag_offset()) &&
            !((*iter)->is_freed()))
        {
            ret = *iter;
            return ret;
        }
    }

    return ret;
}

//----------------------------------------------------------------------
BundleRef
BundleList::find(const GbofId& gbof_id, const BundleTimestamp& extended_id) const
{
    oasys::ScopeLock l(lock_, "BundleList::find");
    BundleRef ret("BundleList::find() temporary (by gbof_id and timestamp)");
    
    for (iterator iter = begin(); iter != end(); ++iter) {
        if (extended_id == (*iter)->extended_id() &&
            gbof_id.equals((*iter)->source(),
                           (*iter)->creation_ts(),
                           (*iter)->is_fragment(),
                           (*iter)->payload().length(),
                           (*iter)->frag_offset()) &&
            !((*iter)->is_freed()))
        {
            ret = *iter;
            return ret;
        }
    }

    return ret;
}

//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
void
BundleList::clear()
{
    oasys::ScopeLock l(lock_, "BundleList::clear");
    
    while (!list_.empty()) {
        BundleRef b("BundleList::clear temporary");
        b = pop_front();
    }
}


//----------------------------------------------------------------------
size_t
BundleList::size() const
{
    oasys::ScopeLock l(lock_, "BundleList::size");
    return list_.size();
}

//----------------------------------------------------------------------
bool
BundleList::empty() const
{
    oasys::ScopeLock l(lock_, "BundleList::empty");
    return list_.empty();
}

//----------------------------------------------------------------------
BundleList::iterator
BundleList::begin() const
{
    if (!lock_->is_locked_by_me())
        PANIC("Must lock BundleList before using iterator");

    // since all list accesses are protected via the BundleList class
    // const/non-const nature, there's no reason to use the stl
    // const_iterator type, so we need to cast away constness
    return const_cast<BundleList*>(this)->list_.begin();
}

//----------------------------------------------------------------------
BundleList::iterator
BundleList::end() const
{
    if (!lock_->is_locked_by_me())
        PANIC("Must lock BundleList before using iterator");

    // see above
    return const_cast<BundleList*>(this)->list_.end();
}

//----------------------------------------------------------------------
BlockingBundleList::BlockingBundleList(const std::string& name)
    : BundleList(name)
{
    notifier_ = new oasys::Notifier(logpath_);
}

//----------------------------------------------------------------------
BlockingBundleList::~BlockingBundleList()
{
    delete notifier_;
    notifier_ = NULL;
}

//----------------------------------------------------------------------
BundleRef
BlockingBundleList::pop_blocking(int timeout)
{
    ASSERT(notifier_);

    log_debug("pop_blocking on list %p [%s]", this, name().c_str());
    
    lock_->lock("BlockingBundleList::pop_blocking");

    bool used_wait;
    if (empty()) {
        used_wait = true;
        bool notified = notifier_->wait(lock_, timeout);
        ASSERT(lock_->is_locked_by_me());

        // if the timeout occurred, wait returns false, so there's
        // still nothing on the list
        if (!notified) {
            lock_->unlock();
            log_debug("pop_blocking timeout on list %p", this);

            return BundleRef("BlockingBundleList::pop_blocking temporary");
        }
    } else {
        used_wait = false;
    }
    
    // This can't be empty if we got notified, unless there is another
    // thread waiting on the queue - which is an error.
    ASSERT(!empty());
    
    BundleRef ret("BlockingBundleList::pop_blocking temporary");
    ret = pop_front(used_wait);
    
    lock_->unlock();

    log_debug("pop_blocking got bundle %p from list %p", 
              ret.object(), this);
    
    return ret;
}
//----------------------------------------------------------------------
void
BundleList::serialize(oasys::SerializeAction *a)
{
    u_int32_t bid;
    u_int sz = size();

    oasys::ScopeLock l2(lock_, "serialize");

    a->process("size", &sz);

    if (a->action_code() == oasys::Serialize::MARSHAL || \
        a->action_code() == oasys::Serialize::INFO) {

        BundleList::iterator i;

        for (i=list_.begin();
             i!=list_.end();
             ++i) {
            log_debug("List %s Marshaling bundle id %d",
            		name().c_str(), (*i)->bundleid());
            bid = (*i)->bundleid();
            a->process("element", &bid);
        }
    }

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        for ( size_t i=0; i<sz; i++ ) {
            a->process("element", &bid);
            log_debug("Unmarshaling bundle id %d", bid);
            BundleRef b = BundleDaemon::instance()->all_bundles()->find(bid);
            if ( b==NULL ) {
                log_debug("b is NULL for id %d; not on all_bundles list", bid);
            }
            if ( b!=NULL && b->bundleid()==bid ) {
                log_debug("Found bundle id %d on all_bundles; adding.", bid);
                push_back(b);
            }
            log_debug("Done with UNMARSHAL %d", bid);
        }
        log_debug("Done with UNMARSHAL");
    }
}


} // namespace dtn

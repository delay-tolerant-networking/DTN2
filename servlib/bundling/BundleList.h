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
#ifndef _BUNDLE_LIST_H_
#define _BUNDLE_LIST_H_

#include <list>
#include <oasys/compat/inttypes.h>
#include <oasys/thread/Notifier.h>

namespace oasys {
class SpinLock;
}

namespace dtn {

class Bundle;
class BundleMapping;

/**
 * List structure for handling bundles.
 *
 * All list operations are protected with a spin lock so they are
 * thread safe. In addition, the lock itself is exposed so a routine
 * that needs to perform more complicated functions (like scanning the
 * list) should lock the list before doing so.
 *
 * The internal data structure is an STL list of Bundle pointers. The
 * list is also derived from Notifier, and the various push() calls
 * will call notify() if there is a thread blocked on an empty list
 * waiting for notification.
 *
 * List methods also maintain the set of mappings in the Bundle class
 * for the lists that contain it. In all cases, the mapping structure
 * is copied when the bundle is added to the list.
 *
 * Furthermore, lists follow the reference counting rules for bundles.
 * In particular, the push*() methods increment the reference count,
 * and remove() decrements it. However, it is important to note that
 * the pop*() methods do not decrement the reference count (though
 * they do update the back pointers), and it is the responsibility of
 * the caller of one of the pop*() methods to explicitly decrement the
 * reference count.
 * 
 */

class BundleList : public oasys::Notifier {
public:
    /**
     * Type for the list itself.
     */
    typedef std::list<Bundle*> ListType;

    /**
     * Type for an iterator.
     */
    typedef ListType::iterator iterator;
    
    /**
     * Type for a const iterator.
     */
    typedef ListType::const_iterator const_iterator;

    /**
     * Constructor
     */
    BundleList(const std::string& name);

    /**
     * Destructor -- clears the list.
     */
    virtual ~BundleList();

    /**
     * Peek at the first bundle on the list.
     *
     * @return the bundle or NULL if the list is empty
     */
    Bundle* front();

    /**
     * Peek at the last bundle on the list.
     *
     * @return the bundle or NULL if the list is empty
     */
    Bundle* back();

    /**
     * Add a new bundle to the front of the list.
     */
    void push_front(Bundle* bundle, const BundleMapping* mapping_info);

    /**
     * Add a new bundle to the front of the list.
     */
    void push_back(Bundle* bundle, const BundleMapping* mapping_info);

    /**
     * Type codes for sorted insertion
     */
    typedef enum {
        SORT_PRIORITY = 0x1,	///< Sort by bundle priority
        SORT_FRAG_OFFSET	///< Sort by fragment offset
    } sort_order_t;
        
    /**
     * Insert the given bundle sorted by the given sort method.
     */
    void insert_sorted(Bundle* bundle, const BundleMapping* mapping_info,
                       sort_order_t sort_order);
    
    /**
     * Remove (and return) the first bundle on the list.
     *
     * Note (as explained above) that this does not decrement the
     * bundle reference count.
     *
     * @return the bundle or NULL if the list is empty. If the
     * mappingp pointer is non-null, the old mapping is returned as
     * well, otherwise it is deleted.
     */
    Bundle* pop_front(BundleMapping** mappingp = NULL);

    /**
     * Remove (and return) the last bundle on the list.
     *
     * Note (as explained above) that this does not decrement the
     * bundle reference count.
     *
     * @return the bundle or NULL if the list is empty. If the
     * mappingp pointer is non-null, the old mapping is returned as
     * well, otherwise it is deleted.
     */
    Bundle* pop_back(BundleMapping** mappingp = NULL);

    /**
     * Remove (and return) the first bundle on the list, blocking
     * (potentially limited by the given timeout) if there are none.
     *
     * Note (as explained above) that this does not decrement the
     * bundle reference count.
     *
     * @return the bundle or NULL if the timeout occurred. If the
     * mappingp pointer is non-null, the old mapping is returned as
     * well, otherwise it is deleted.
     */
    Bundle* pop_blocking(BundleMapping** mappingp = NULL,
                         int timeout = -1);
    /**
     * Remove the bundle at the given list position. Always succeeds
     * since the iterator must be valid.
     *
     * Unlike the pop() functions, this does remove the list's
     * reference on the bundle.

     * If the mappingp pointer is non-null, the old mapping is
     * returned as well, otherwise it is deleted.
     */
    void erase(iterator& pos, BundleMapping** mappingp = NULL);

    /**
     * Search the list for a bundle with the given id.
     *
     * @return the bundle or NULL if not found.
     */
    Bundle* find(u_int32_t bundleid);

    /**
     * Move all bundles from this list to another.
     */
    void move_contents(BundleList* other);

    /**
     * Clear out the list.
     */
    void clear();

    /**
     * Return the size of the list.
     */
    size_t size() const;

    /**
     * Iterator used to iterate through the list. Iterations _must_ be
     * completed while holding the list lock, and this method will
     * assert as such.
     */
    iterator begin();
    
    /**
     * Iterator used to mark the end of the list. Iterations _must_ be
     * completed while holding the list lock, and this method will
     * assert as such.
     */
    iterator end();

    /**
     * Const iterator used to iterate through the list. Iterations
     * _must_ be completed while holding the list lock, and this
     * method will assert as such.
     */
    const_iterator begin() const;

    /**
     * Const iterator used to mark the end of the list. Iterations
     * _must_ be completed while holding the list lock, and this
     * method will assert as such.
     */
    const_iterator end() const;

    /**
     * Return the internal lock on this list.
     */
    oasys::SpinLock* lock() const { return lock_; }

    /**
     * Return the identifier name of this list.
     */
    const std::string& name() const { return name_; }
    
protected:
    /**
     * Helper routine to add a bundle at the indicated position.
     */
    void add_bundle(Bundle* bundle, iterator pos,
                    const BundleMapping* mapping_info);

    /**
     * Helper routine to remove a bundle from the indicated position.
     *
     * @returns the bundle that, before this call, was at the position
     */
    Bundle* del_bundle(iterator pos, BundleMapping** mappingp);
    
    oasys::SpinLock* lock_;
    ListType list_;
    std::string name_;
};

} // namespace dtn

#endif /* _BUNDLE_LIST_H_ */

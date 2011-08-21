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

#ifndef _BUNDLE_LIST_H_
#define _BUNDLE_LIST_H_

#include <list>
#include <oasys/compat/inttypes.h>
#include <oasys/thread/Notifier.h>
#include <oasys/serialize/Serialize.h>

#include "BundleRef.h"
#include "naming/EndpointID.h"
#include "GbofId.h"

namespace oasys {
class SpinLock;
}

namespace dtn {

class Bundle;
class BundleTimestamp;

/**
 * List structure for handling bundles.
 *
 * All list operations are protected with a spin lock so they are
 * thread safe. In addition, the lock itself is exposed so a routine
 * that needs to perform more complicated functions (like scanning the
 * list) must lock the list before doing so.
 *
 * NOTE: It is important that no locks be held on any bundles that
 * might be contained within the list when calling list manipulation
 * functions. Doing so could cause a potential deadlock due to a lock
 * ordering violation, as the list manipulation routines first lock
 * the list, and then lock the bundle(s) being added or removed. 
 *
 * The internal data structure is an STL list of Bundle pointers. The
 * list is also derived from Notifier, and the various push() calls
 * will call notify() if there is a thread blocked on an empty list
 * waiting for notification.
 *
 * List methods also maintain mappings (i.e. "back pointers") in each
 * Bundle instance to the set of lists that contain the bundle.
 *
 * Lists follow the reference counting rules for bundles. In
 * particular, the push*() methods increment the reference count, and
 * erase() decrements it. In particular, the pop() variants (as well
 * as the other accessors) return instances of the BundleRef classes
 * that forces the caller to use the BundleRef classes as well in
 * order to properly maintain the reference counts.
 *
 */
class BundleList : public oasys::Logger,  public oasys::SerializableObject {
private:
    /**
     * Type for the list itself (private since it's irrelevant to the
     * outside).
     */
    typedef std::list<Bundle*> List;

public:
    /**
     * Type for an iterator, which just wraps an stl iterator. We
     * don't ever use the stl const_iterator type since list mutations
     * are protected via this class' methods.
     */
    typedef List::iterator iterator;

    /**
     * Constructor
     */
    BundleList(const std::string& name, oasys::SpinLock* lock = NULL);

    /**
     * Destructor -- clears the list.
     */
    virtual ~BundleList();

    /**
     * Peek at the first bundle on the list.
     *
     * @return the bundle or NULL if the list is empty
     */
    BundleRef front() const;

    /*
     * Serializes the list of (internal) bundleIDs; on deserialization,
     * tries to hunt down those bundles in the all_bundles list and add
     * them to the list.
     */
    void serialize(oasys::SerializeAction *a);

    /**
     * Peek at the last bundle on the list.
     *
     * @return the bundle or NULL if the list is empty
     */
    BundleRef back() const;

    /**
     * Add a new bundle to the front of the list.
     */
    void push_front(Bundle* bundle);

    /**
     * Add a new bundle to the front of the list.
     */
    void push_front(const BundleRef& bundle)
    {
        return push_front(bundle.object());
    }

    /**
     * Add a new bundle to the back of the list.
     */
    void push_back(Bundle* bundle);

    /**
     * Add a new bundle to the back of the list.
     */
    void push_back(const BundleRef& bundle)
    {
        return push_back(bundle.object());
    }

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
    void insert_sorted(Bundle* bundle, sort_order_t sort_order);

    /**
     * As a testing hook, insert the given bundle into a random
     * location in the list.
     */
    void insert_random(Bundle* bundle);
    
    /**
     * Remove (and return) a reference to the first bundle on the list.
     *
     * @param used_notifier Popping off of the BundleList after coming 
     *     off of a notifier. This will drain one item off of the 
     *     notifier queue.
     *                    
     * @return a reference to the bundle or a reference to NULL if the
     * list is empty.
     */
    BundleRef pop_front(bool used_notifier = false);

    /**
     * Remove (and return) the last bundle on the list.
     *
     * Note (as explained above) that this does not decrement the
     * bundle reference count.
     *
     * @param used_notifier Popping off of the BundleList after coming 
     *     off of a notifier. This will drain one item off of the 
     *     notifier queue.
     *
     * @return a reference to the bundle or a reference to NULL if the
     * list is empty.
     */
    BundleRef pop_back(bool used_notifier = false);

    /**
     * Remove the given bundle from the list. Returns true if the
     * bundle was successfully removed, false otherwise.
     *
     * Unlike the pop() functions, this does remove the list's
     * reference on the bundle.
     */
    bool erase(Bundle* bundle, bool used_notifier = false);

    bool erase(const BundleRef& bundle, bool used_notifier = false) {
        return erase(bundle.object(), used_notifier);
    }

    /**
     * Remove the bundle at the given list position.
     */
    void erase(iterator& iter, bool used_notifier = false);
    
    /**
     * Search the list for the given bundle.
     *
     * @return true if found, false if not
     */
    bool contains(Bundle* bundle) const;

    /**
     * Search the list for the given bundle.
     *
     * @return true if found, false if not
     */
    bool contains(const BundleRef& bundle) const
    {
        return contains(bundle.object());
    }
    
    /**
     * Search the list for a bundle with the given id.
     *
     * @return a reference to the bundle or a reference to NULL if the
     * list is empty.
     */
    BundleRef find(u_int32_t bundleid) const;

    /**
     * Search the list for a bundle with the given source eid and
     * timestamp.
     *
     * @return a reference to the bundle or a reference to NULL if the
     * list is empty.
     */
    BundleRef find(const EndpointID& source_eid,
                   const BundleTimestamp& creation_ts) const;

    /**
     * Search the list for a bundle with the given GBOF ID
     *
     * @return the bundle or NULL if not found.
     */
    BundleRef find(GbofId& gbof_id) const;
    
    /**
     * Search the list for a bundle with the given GBOF ID and extended
     * (local) ID
     *
     * @return the bundle or NULL if not found.
     */
    BundleRef find(const GbofId& gbof_id,
                   const BundleTimestamp& extended_id) const;

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
     * Return whether or not the list is empty.
     */
    bool empty() const;

    /**
     * Iterator used to iterate through the list. Iterations _must_ be
     * completed while holding the list lock, and this method will
     * assert as such.
     */
    iterator begin() const;
    
    /**
     * Iterator used to mark the end of the list. Iterations _must_ be
     * completed while holding the list lock, and this method will
     * assert as such.
     */
    iterator end() const;

    /**
     * Return the internal lock on this list.
     */
    oasys::SpinLock* lock() const { return lock_; }

    /**
     * Return the identifier name of this list.
     */
    const std::string& name() const { return name_; }

    /**
     * Set the name (useful for classes that are unserialized).
     * Also sets the logpath
     */
    void set_name(const std::string& name);
    
    /**
     * As above, but sets ONLY the name, not the logpath.
     */
    void set_name_only(const std::string& name);

private:
    /**
     * Helper routine to add a bundle at the indicated position.
     */
    void add_bundle(Bundle* bundle, const iterator& pos);
    
    /**
     * Helper routine to remove a bundle from the indicated position.
     *
     * @param pos	    Position to delete
     * @param used_notifier Popping off of the BundleList after coming
     *                      off of a notifier. This will drain one item
     *                      off of the notifier queue.
     *
     * @returns the bundle that, before this call, was at the position
     */
    Bundle* del_bundle(const iterator& pos, bool used_notifier);
    
    std::string      name_;	///< name of the list
    List             list_;	///< underlying list data structure
    
protected:
    oasys::SpinLock* lock_;	///< lock for notifier
    bool             own_lock_; ///< bit to define lock ownership
    oasys::Notifier* notifier_; ///< notifier for blocking list
};

/**
 * A simple derivative to the BundleList class that hooks in an oasys
 * Notifier, which thereby allows inter-thread signalling via a
 * pop_blocking() method. This allows one thread to block until
 * another has added a bundle to the list.
 */
class BlockingBundleList : public BundleList {
public:
    BlockingBundleList(const std::string& name);

    virtual ~BlockingBundleList();
    
    /**
     * Remove (and return) the first bundle on the list, blocking
     * (potentially limited by the given timeout) if there are none.
     *
     * @return a reference to the bundle or a reference to NULL if the
     * list is empty.
     */
    BundleRef pop_blocking(int timeout = -1);

    /**
     * Accessor for the internal notifier.
     */
    oasys::Notifier* notifier() { return notifier_; }
};

} // namespace dtn

#endif /* _BUNDLE_LIST_H_ */

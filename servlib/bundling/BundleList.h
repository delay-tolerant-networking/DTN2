#ifndef _BUNDLE_LIST_H_
#define _BUNDLE_LIST_H_

#include <list>
#include "thread/Notifier.h"

class Bundle;
class SpinLock;

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
 * List methods also maintain the set of back pointers in the Bundle
 * class for the lists that contain it, and follow the reference
 * counting rules for bundles. In particular, the push*() methods
 * increment the reference count, and remove() decrements it. However,
 * it is important to note that the pop*() methods do not decrement
 * the reference count (though they do update the back pointers), and
 * it is the responsibility of the caller of one of the pop*() methods
 * to explicitly decrement the reference count.
 */
class BundleList : public Notifier {
public:
    BundleList(const std::string& name);
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
    void push_front(Bundle* bundle);

    /**
     * Add a new bundle to the front of the list.
     */
    void push_back(Bundle* bundle);

    /**
     * Remove (and return) the first bundle on the list.
     *
     * Note (as explained above) that this does not decrement the
     * bundle reference count.
     *
     * @return the bundle or NULL if the list is empty
     */
    Bundle* pop_front();

    /**
     * Remove (and return) the last bundle on the list.
     *
     * Note (as explained above) that this does not decrement the
     * bundle reference count.
     *
     * @return the bundle or NULL if the list is empty
     */
    Bundle* pop_back();

    /**
     * Remove (and return) the first bundle on the list, blocking if
     * there are none.
     *
     * Note (as explained above) that this does not decrement the
     * bundle reference count.
     *
     * @return the bundle or NULL if the list is empty
     */
    Bundle* pop_blocking();

    /**
     * Remove the given bundle.
     *
     * @return true if the bundle was removed successfully, false if
     * it was not on the list
     */
    bool remove(Bundle* bundle);
    
    /**
     * Return the size of the list.
     */
    size_t size();

    /**
     * Type for an iterator.
     */
    typedef std::list<Bundle*>::iterator iterator;

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
     * Return the internal lock on this list.
     */
    SpinLock* lock() { return lock_; }

    /**
     * Return the identifier name of this list.
     */
    const std::string& name() const { return name_; }
    
protected:
    SpinLock* lock_;
    std::list<Bundle*> list_;
    std::string name_;
};

#endif /* _BUNDLE_LIST_H_ */

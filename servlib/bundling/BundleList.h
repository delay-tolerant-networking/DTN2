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
 * The internal data structure is just an STL list of Bundle pointers,
 * and in general, the exported functions just expose the internal STL
 * counterpart. In particular, however, the pop_* functions return the
 * bundle that was on the end (front or back) of the list so the
 * operation can be atomic (protected by the spin lock).
 *
 * The list is also derived from Notifier, and the various push()
 * calls will call notify() so threads can block on an empty list
 * waiting for notification.
 */
class BundleList : public Notifier {
public:
    BundleList();
    virtual ~BundleList();

    /**
     * Peek at the first bundle on the list.
     */
    Bundle* front();

    /**
     * Peek at the last bundle on the list.
     */
    Bundle* back();

    /**
     * Remove (and return) the first bundle on the list.
     */
    Bundle* pop_front();

    /**
     * Remove (and return) the last bundle on the list.
     */
    Bundle* pop_back();

    /**
     * Remove (and return) the first bundle on the list, blocking if
     * there are none.
     */
    Bundle* pop_blocking();
    
    /**
     * Add a new bundle to the front of the list.
     */
    void push_front(Bundle* bundle);

    /**
     * Add a new bundle to the front of the list.
     */
    void push_back(Bundle* bundle);

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
     * Return an indication if there are any threads blocked waiting
     * for bundles.
     */
    bool waiter() { return waiter_; }
    
protected:
    SpinLock* lock_;
    std::list<Bundle*> list_;
    bool waiter_;
};

#endif /* _BUNDLE_LIST_H_ */

/*
 *    Copyright 2007 Baylor University
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

#ifndef _PROPHET_REPOSITORY_H_
#define _PROPHET_REPOSITORY_H_

#include "Bundle.h"
#include "BundleList.h"
#include "QueuePolicy.h"

#if defined(__GNUC__)
# define PRINTFLIKE(fmt, arg) __attribute__((format (printf, fmt, arg)))
#else
# define PRINTFLIKE(a, b)
#endif

namespace prophet
{

/**
 * Implements a modified heap-based priority_queue with bounds enforcement.
 * Any change to a Bundle's priority (such as the act of forwarding a Bundle
 * over a link) requires a call-back to change_priority to preserve correct
 * heap ordering.  Any change to a Bundle's size will result in undefined
 * behavior; at present, the best practice is to drop() the bundle before
 * the size change then add() it again after the size is finalized. Any
 * change in policy (ie, a new comparator) will cost n for the pass-thru
 * of reheaping; any change in max will also be at most a linear cost.
 */
class Repository
{
public:
    typedef BundleList::iterator iterator;
    typedef BundleList::const_iterator const_iterator;

    /**
     * Reduced interface into BundleCore to provide logging, drop_bundle
     * signal, and answer the query for bundle storage quota
     */
    class BundleCoreRep
    {
    public:
        virtual ~BundleCoreRep() {}
        virtual void print_log(const char* name, int level,
                               const char* fmt, ...)
            PRINTFLIKE(4,5) = 0;
        virtual void drop_bundle(const Bundle* bundle) = 0;
        virtual u_int64_t max_bundle_quota() const = 0;
    };

    /**
     * Constructor. Repository assumes ownership of memory pointed to by qc, 
     * but not for list (makes a copy).
     * @param core  facade interface into Bundle host
     * @param qc	Queue Policy comparator to sort by eviction order (such
     *              that the next Bundle to be evicted is evaluated less-than
     *              all other Bundles)
     * @param list	initial list of Bundles to store
     */
    Repository(BundleCoreRep* core, QueueComp* qc = NULL,
               const BundleList* list = NULL);

    /**
     * Destructor
     */
    ~Repository();

    /**
     * Add bundle to Repository, incrementing current utilization by 
     * Bundle's storage size.
     * @param bundle	pointer to Bundle to be added
     * @return whether bundle was added
     */
    bool add(const Bundle* bundle);

    /**
     * Remove arbitrary Bundle from Repository, and decrement current
     * utilization accordingly. Assumes Bundle priority has not been
     * changed, and that underlying heap is in priority order.
     * @param bundle	pointer to Bundle to be removed
     */
    void del(const Bundle* bundle);

    /**
     * Change policy of Repository by replacing comparator; post condition
     * is that eviction order is resorted using the new comparator.
     * @param qc	comparator used to sort Bundles in eviction order 
     */
    void set_comparator(QueueComp* qc);

    /**
     * Accessor to current comparator
     */
    const QueueComp* get_comparator() const { return comp_; }

    /**
     * Callback to instruct Repository to query BundleCore on new max
     */
    void handle_change_max();

    /**
     * Callback for external notice to recalculate eviction order for
     * list, due to changed comparator state
     */
    void change_priority(const Bundle* b);

    /**
     * Fetch const reference to internal list of Bundles (there is no
     * guarantee of meaningful order to returned list)
     */
    const BundleList& get_bundles() const { return list_; }

    /**
     * Return boolean indicating whether Repository has any bundles.
     */
    bool empty() const { return list_.empty(); }

    /**
     * Return number of Bundles in Repository
     */
    size_t size() const { return list_.size(); }

    /**
     * Return the current upper limit imposed by Repository
     */
    u_int get_max() const { return core_->max_bundle_quota(); }

    /**
     * Return current storage consumed by Bundles in Repository
     */
    u_int get_current() const { return current_; }

protected:
    /**
     * Evict the next candidate Bundle according to policy order
     */
    void evict();

    ///@{ Heap operations
    void make_heap(size_t first, size_t last);
    void push_heap(size_t first, size_t hole, size_t top, const Bundle* b);
    void pop_heap(size_t first, size_t last, size_t result, const Bundle* b);
    void adjust_heap(size_t first, size_t hole, size_t len, const Bundle* b);
    void remove_and_reheap(size_t hole);
    ///@}

    /**
     * Utility function for find
     */
    bool find(const Bundle* b, iterator& i);

    BundleCoreRep* core_; ///< facade interface into Bundle host
    QueueComp* comp_; ///< queue policy Bundle comparator
    BundleList list_; ///< array-based eviction-ordered heap of Bundles
    u_int current_;   ///< current utilization
}; // class Repository

}; // namespace prophet

#endif // _PROPHET_REPOSITORY_H_

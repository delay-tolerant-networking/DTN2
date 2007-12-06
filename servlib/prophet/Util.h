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

#ifndef _PROPHET_UTIL_H_
#define _PROPHET_UTIL_H_

#include <string>
#include <algorithm>
#include <vector>

namespace prophet
{

#define FOUR_BYTE_ALIGN(x) (((x) % 4) != 0) ? ((x) + (4 - ((x) % 4))) : (x)

struct less_string : public std::less<std::string>
{
    bool operator() (const std::string& a, const std::string& b) const
    {
        return (a.compare(b) < 0);
    }
};

template<typename T,typename size_type=size_t>
struct DoNothing
{
    inline void operator()(T& t, size_type pos)
    {
        (void)t;
        (void)pos;
    }
};

#define PARENT(_x) (((_x) - 1) >> 1)
#define LEFT(_x)   (((_x) << 1) + 1)

template <typename UnitType,
          typename Sequence=std::vector<UnitType>,
          typename Compare=std::less<typename Sequence::value_type>,
          typename UpdateElem=DoNothing<UnitType,typename Sequence::size_type> >
class Heap
{
public:
    typedef typename Sequence::value_type      value_type;
    typedef typename Sequence::size_type       size_type;
    typedef typename Sequence::iterator        iterator;
    typedef typename Sequence::const_reference const_reference;

    /**
     * Default constructor
     */
    Heap()
    {
        comp_ = new Compare();
    }

    /**
     * Heap assumes ownership of comp
     */
    Heap(Compare* comp) 
        : comp_(comp)
    {
    }

    /**
     * Destructor
     */
    virtual ~Heap() { delete comp_; }

    /**
     * Constant reference to current compare 
     */
    inline
    const Compare* compare() const { return comp_; }

    /**
     * Change to new compare and reorder heap
     */
    inline
    void set_compare(Compare* comp) {
        delete comp_;
        comp_ = comp;
        make_heap(0);
        update_elements();
    }

    /**
     * Verify heap order
     */
    inline
    static bool is_heap(const Sequence& list, Compare& comp) {
        size_type len = list.size();
        size_type parent = 0;
        for (size_type child = 1; child < len; child++) {
            if ((comp)(list[parent],list[child]))
                return false;
            if ((child & 1) == 0)
                parent++;
        }
        return true;
    }

    /**
     * Pass over entire sequence to ensure each element's heap pointer
     */
    inline
    void update_elements() {
        size_type pos = size();
        while (pos-- > 0)
            (upd_)(seq_[pos],pos);
    }

    /**
     * Return true if underlying sequence is empty
     */
    bool empty() const { return seq_.empty(); }
    
    /**
     * Return number of elements in underlying sequence
     */
    size_type size() const { return seq_.size(); }

    /**
     * Return read-only reference to top of heap
     */
    const_reference top() const { return seq_.front(); }

    /**
     * Add data to heap, return index of insertion point
     */
    inline
    void add(value_type x) {
        // first add value to end of underlying sequence
        seq_.push_back(x);
        // re-order sequence into heap
        size_type pos = heap_up(size() - 1);
        // update element with new heap position
        (upd_)(seq_[pos],pos);
    }

    /**
     * Remove element from heap by position, re-heap remaining 
     * elements
     */
    inline
    void remove(size_t pos) {
        // overwrite victim with last element
        seq_[pos] = seq_[size()-1];
        (upd_)(seq_[pos],pos);
        // remove last element
        seq_.pop_back();
        // reheap everything down from pos
        heap_down(pos);
    }

    /**
     * Constant reference to underlying sequence
     */
    const Sequence& sequence() const { return seq_; }

protected:
    /**
     * Assuming left and right subtrees are in heap order, push hole
     * down the tree until it reaches heap order
     */
    inline
    size_type heap_down(size_type hole) {

        size_type top = hole;
        size_type left = LEFT(top);
        size_type right = left + 1;

        while (left < size()) {

            if ((*comp_)(seq_[hole],seq_[left]))
                top = left;

            if (right < size() && (*comp_)(seq_[top],seq_[right]))
                top = right;

            // heap order throughout, we're done
            if (top == hole)
                break;

            // swap top and hole
            swap(top,hole);

            // reset values for next iteration
            hole = top;
            left = LEFT(top);
            right = left + 1;
        }
        return top;
    }

    /**
     * Assuming tree is heap order above last, bubble up last until
     * the tree is in heap order
     */
    inline
    size_type heap_up(size_type last) {

        size_type parent = PARENT(last);

        while (last > 0) {

            // check for heap order
            if (!(*comp_)(seq_[parent],seq_[last]))
                break;

            swap(parent,last);

            last = parent;
            parent = PARENT(last);
        }
        return last;
    }

    /**
     * Begin with last node (a leaf, therefore a subtree of size 1,
     * therefore a heap) and work upwards to build heap
     */
    inline
    void make_heap(size_type first) {

        // first scan down to a leaf node from first
        size_type left = LEFT(first);
        while (left < size() - 1)
            left = LEFT(left);

        // now work back up towards first
        size_type parent = PARENT(left);
        while (true) {
            heap_down(parent);
            if (parent == first) break;
            parent--;
        }
    }
    
    /**
     * Swap function used by heap_up, heap_down
     */
    inline
    void swap(size_type a, size_type b) {
        std::iter_swap<iterator>(
                seq_.begin() + a,
                seq_.begin() + b);
        (upd_)(seq_[a],a);
        (upd_)(seq_[b],b);
    }

    Sequence seq_;
    Compare* comp_;
    UpdateElem upd_;
}; //template<> class Heap

}; //namespace prophet

#endif // _PROPHET_UTIL_H_

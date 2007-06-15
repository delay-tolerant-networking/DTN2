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

#include "Repository.h"

#define LOG(_level,_args...) core_->print_log("repository", \
        BundleCore::_level, _args);

namespace prophet
{

Repository::Repository(BundleCoreRep* core,
                       QueueComp* qc,
                       const BundleList* list)
    : core_(core), comp_(NULL), current_(0)
{
    // default comparator is FIFO
    if (qc == NULL)
    {
        comp_ = QueuePolicy::policy(QueuePolicy::FIFO);
    }
    else
    {
        comp_ = qc;
    }

    if (list != NULL)
        for(const_iterator i = list->begin(); i != list->end(); i++)
            add(*i);
}

Repository::~Repository()
{
    delete comp_;
}

void
Repository::del(const Bundle* b)
{
    LOG(LOG_DEBUG,"del request");
    if (b == NULL)
    {
        LOG(LOG_DEBUG,"NULL bundle"); 
        return;
    }

    // first get iterator for b
    iterator i;
    if (find(b,i))
    {
        // parameter may only have index set, for find
        // so grab the reference returned by find
        b = *i;
        // decrement utilization by Bundle's size
        current_ -= b->size();
        // reorder sequence to preserve eviction ordering, moving
        // victim to last pos in vector
        remove_and_reheap(i-list_.begin());
        // remove victim from vector
        list_.pop_back();
        LOG(LOG_DEBUG,"removed %d from list",b->sequence_num());
    }
}

bool
Repository::add(const Bundle* b)
{
    LOG(LOG_DEBUG, "add request");
    if (b == NULL)
    {
        LOG(LOG_DEBUG,"NULL bundle");
        return false;
    }

    // duplicates not allowed
    iterator i;
    if (find(b,i))
        return false;

    // add to underlying sequence
    list_.push_back(b);
    // reorder sequence to eviction order
    size_t last_pos = list_.size() - 1;
    push_heap(0,last_pos,0,list_[last_pos]);
    // increment utilization by this Bundle's size
    current_ += b->size();
    // maintain quota
    if (core_->max_bundle_quota() > 0)
        while (core_->max_bundle_quota() < current_)
            evict();
    return true;
}

void
Repository::set_comparator(QueueComp* qc)
{
    if (qc == NULL) return;
    LOG(LOG_DEBUG,"changing policy from %s to %s",
            QueuePolicy::qp_to_str(comp_->qp()),
            QueuePolicy::qp_to_str(qc->qp()));
    // clean up memory
    delete comp_;
    comp_ = qc;
    // recalculate eviction order based on new comp_
    if (!list_.empty())
        make_heap(0, (list_.size() - 1));
}

void
Repository::handle_change_max()
{
    // enforce the new quota
    if (core_->max_bundle_quota() > 0)
        while (core_->max_bundle_quota() < current_)
            evict();
}

void
Repository::change_priority(const Bundle* b)
{
    LOG(LOG_DEBUG,"change priority request %d",
        b == NULL ? 0 : b->sequence_num());

    if (b == NULL)
        return;

    iterator i = list_.begin();
    // brute-force search since heap property is violated
    while (i != list_.end())
        if (*(*i) == *b)
            break;
        else i++;

    if (i != list_.end())
    {
        // pull bundle out of the heap and stick it at the end of list_
        remove_and_reheap(i - list_.begin());
        // pull bundle from end of list_ and put it in heapified pos
        push_heap(0,list_.size() - 1,0,b);
    }
}

void
Repository::evict()
{
    if (comp_->qp() != QueuePolicy::LEPR)
    {
do_evict:
        size_t last_pos = list_.size() - 1;
        // re-order so that eviction candidate is now at the end of list_
        pop_heap(0, last_pos, last_pos, list_[last_pos]);
        // capture a pointer to the back of list_
        const Bundle* b = list_.back();
        // drop the last member off the end of list_
        list_.pop_back();
        // callback into Bundle core to request deletion of Bundle
        core_->drop_bundle(b);
        // decrement current consumption by Bundle's size
        current_ -= b->size();

        return;
    }
    else
    {
        // LEPR adds the burden of checking for NF > min_NF

        // search heap tree from top down, left to right (linearly thru vector)
        size_t len = list_.size();
        for (size_t pos = 0; pos < len; pos++)
        {
            const Bundle* b = list_[pos];
            if (comp_->min_fwd_ < b->num_forward())
            {
                // victim is found, now evict
                // decrement utilization by Bundle's size
                current_ -= b->size();
                // reorder sequence to preserve eviction ordering, moving
                // victim to last pos in vector
                remove_and_reheap(pos);
                // remove victim from vector
                list_.pop_back();
                return;
            }
        }
        // Here's where Prophet doesn't say what to do: the entire heap was
        // searched, but no victims qualified, due to the min_NF constraint.
        // Since eviction must happen, then override the min_NF requirement
        // and let's go ahead and evict top()
        // This stinks; we've already paid linear, now we pay another log on
        // top of that.
        goto do_evict;
    }
}

void
Repository::push_heap(size_t first, size_t hole, size_t top, const Bundle* b)
{
    size_t parent = (hole - 1) / 2;
    while (hole > top && (*comp_)(list_[first + parent],b))
    {
        list_[first + hole] = list_[first + parent];
        hole = parent;
        parent = (hole - 1) / 2;
    }
    list_[first + hole] = b;
}

void
Repository::pop_heap(size_t first, size_t last, size_t result, const Bundle* b)
{
    list_[result] = list_[first];
    adjust_heap(first, 0, last - first, b);
}

void
Repository::adjust_heap(size_t first, size_t hole, size_t len, const Bundle* b)
{
    // size 0 or 1 is already a valid heap!
    if (list_.size() < 2)
        return;

    const size_t top = hole;
    size_t second = 2 * hole + 2;
    while (second < len)
    {
        if ((*comp_)(list_[first + second], list_[first + (second - 1)]))
            second--;
        list_[first + hole] = list_[first + second];
        hole = second;
        second = 2 * (second + 1);
    }
    if (second == len)
    {
        list_[first + hole] = list_[first + (second - 1)];
        hole = second - 1;
    }
    push_heap(first, hole, top, b);
}

void
Repository::remove_and_reheap(size_t hole)
{
    // overwrite victim with lowest ranking leaf
    list_[hole] = list_[list_.size() - 1];
    // reheap downstream from hole
    adjust_heap(0, hole, list_.size() - 2, list_[hole]);
}

void
Repository::make_heap(size_t first, size_t last)
{
    // size 0 or 1 is already a valid heap!
    if (last < first + 2) return;

    size_t len = last - first;
    size_t parent = (len - 2) / 2;
    while (true)
    {
        adjust_heap(first, parent, len, list_[first + parent]);
        if (parent == 0) break;
        parent--;
    }
}

bool
Repository::find(const Bundle* b, iterator& i)
{
    if (list_.empty()) return false;

    i = list_.begin();
    while (i != list_.end())
        if (b == *i)
            break;
        else
            i++;
    return (i != list_.end() && b == *i);
}

}; // namespace prophet

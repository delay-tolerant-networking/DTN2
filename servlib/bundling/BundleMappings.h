/*
 *    Copyright 2007 Intel Corporation
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

#ifndef _BUNDLE_MAPPING_H_
#define _BUNDLE_MAPPING_H_

#include <vector>

#include "BundleList.h"

namespace dtn {

/**
 * Structure stored in a list along with each bundle to keep a
 * "backpointer" to any bundle lists that the bundle is queued on to
 * make searching the lists more efficient.
 *
 * Relies on the fact that BundleList::iterator remains valid through
 * insertions and removals to other parts of the list.
 */
class BundleMapping {
public:
    BundleMapping() : list_(NULL), position_() {}
    
    BundleMapping(BundleList* list, const BundleList::iterator& position)
        : list_(list), position_(position) {}

    BundleList*                 list()     const { return list_; }
    const BundleList::iterator& position() const { return position_; }

protected:
    /// Pointer to the list on which the bundle is held
    BundleList* list_;

    /// Position of the bundle on that list
    BundleList::iterator position_;
};

/**
 * Class to define the set of mappings.
 *
 * Currently the code just uses a vector to store the mappings to make
 * it compact in memory and because the number of queues for each
 * bundle is likely small.
 */
class BundleMappings : public std::vector<BundleMapping> {
public:
    /**
     * Return an iterator at the mapping to the given list, or end()
     * if the mapping is not present.
     */
    iterator find(const BundleList* list);

    /**
     * Syntactic sugar for finding whether or not a mapping exists for
     * the given list.
     */
    bool contains(const BundleList* list) { return find(list) != end(); }
};

} // namespace dtn

#endif /* _BUNDLE_MAPPING_H_ */

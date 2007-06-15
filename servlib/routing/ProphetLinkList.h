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

#ifndef _PROPHET_LINK_LIST_H_
#define _PROPHET_LINK_LIST_H_

#include <string.h>
#include "ProphetLink.h"

namespace dtn
{

class ProphetLinkList
{
public:
    /**
     * Constructor
     */
    ProphetLinkList();

    /**
     * Destructor
     */
    ~ProphetLinkList();

    /**
     * Add a mapping between DTN LinkRef and prophet::Link*
     */
    void add(const LinkRef& l);

    /**
     * Remove mapping between DTN LinkRef and prophet::Link*
     */
    void del(const LinkRef& l);

    /**
     * Given remote_eid, return pointer to prophet object
     */
    const prophet::Link* find(const char* remote_eid) const;

    /**
     * Given prophet object, return dtn::LinkRef&
     */
    const LinkRef& find_ref(const prophet::Link* link) const;

    /**
     * Given remote_eid, return dtn::LinkRef&
     */
    const LinkRef& find_ref(const char* remote_eid) const;

    /**
     * Remove all items from list and clean up memory
     */
    void clear();

    ///@{ Accessors
    bool empty() const { return list_.empty(); }
    size_t size() const { return list_.size(); }
    ///@}

protected:
    typedef std::list<ProphetLink*> List;
    typedef List::iterator iterator;
    typedef List::const_iterator const_iterator;

    /**
     * Internal find utility method
     */
    bool find(const char* remote_eid, iterator& i);

    List list_; ///< collection of ProphetLink's
    static LinkRef NULL_LINK;

}; // class ProphetLinkList

}; // namespace dtn

#endif // _PROPHET_LINK_LIST_H_

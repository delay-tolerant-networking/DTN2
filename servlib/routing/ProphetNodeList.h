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

#ifndef _PROPHET_NODE_LIST_H_
#define _PROPHET_NODE_LIST_H_

#include "prophet/Node.h"
#include "prophet/Table.h"
#include "storage/ProphetStore.h"
#include <oasys/debug/Log.h>

namespace dtn
{

/**
 * Maintain a one-to-one mapping of objects in memory to objects
 * in permanent store, much like a write-thru cache
 */
class ProphetNodeList
{
public:
    /**
     * Constructor
     */
    ProphetNodeList();

    /**
     * Destructor
     */
    virtual ~ProphetNodeList();
    
    /**
     * Deserialize from storage
     */
    void load(const prophet::Node* n);

    /**
     * Update (or add new) node in permanent store
     */
    void update(const prophet::Node* n);

    /**
     * Remove node from permanent store
     */
    void del(const prophet::Node* n);

    /**
     * Retrieve node from permanent store; returns NULL if not found
     */
    const prophet::Node* find(const std::string& dest_id) const;

    /**
     * Copy list of nodes from permanent store into prophet::Table
     */
    void clone(prophet::Table* nodes, const prophet::NodeParams* params);

    /**
     * Clean up memory associated with this list (leaving permanent
     * store untouched)
     */
    void clear();

    ///@{ Accessors
    bool empty() const { return list_.empty(); }
    size_t size() const { return list_.size(); }
    ///@}

protected:
    typedef std::list<prophet::Node*> List;
    typedef List::iterator iterator;
    typedef List::const_iterator const_iterator;

    /**
     * Given primary key, locate node in list
     */
    bool find(const std::string& dest_id, iterator& i);

    List list_; ///< collection of prophet::Node's

}; // class ProphetNodeList

}; // namespace dtn

#endif // _PROPHET_NODE_LIST_H_

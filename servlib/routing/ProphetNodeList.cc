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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif
#include "ProphetNodeList.h"

namespace dtn
{

ProphetNodeList::ProphetNodeList()
{
}

ProphetNodeList::~ProphetNodeList()
{
    clear();
}
    
void
ProphetNodeList::load(const prophet::Node* n)
{
    log_debug_p("/dtn/route/nodelist","load");
    iterator i;
    ASSERT(n != NULL);
    ASSERT(! find(n->dest_id(), i));

    log_debug_p("/dtn/route/nodelist",
            "add new node for %s (age %u pv %.2f flags %s%s%s)",
            n->dest_id(),
            n->age(),
            n->p_value(),
            n->relay() ? "R" : "-",
            n->custody() ? "C" : "-",
            n->internet_gw() ? "I" : "-");
    // create and insert new object
    ProphetNode* a = new ProphetNode(*n);
    // internally and in ProphetStorage
    list_.insert(i,a);
}

void
ProphetNodeList::update(const prophet::Node* n)
{
    log_debug_p("/dtn/route/nodelist","update");
    iterator i;
    ASSERT(n != NULL);
    if (! find(n->dest_id(), i))
    {
        log_debug_p("/dtn/route/nodelist",
                "add new node for %s",n->dest_id());
        // create and insert new object
        ProphetNode* a = new ProphetNode(*n);
        // internally and in ProphetStorage
        list_.insert(i,a);
        ProphetStore::instance()->add(a);
    }
    else
    {
        log_debug_p("/dtn/route/nodelist",
                "update existing node for %s",n->dest_id());
        // update existing
        ProphetNode* a = static_cast<ProphetNode*>(*i);
        a->set_pvalue( n->p_value() );
        ProphetStore::instance()->update(a);
    }
}

void
ProphetNodeList::del(const prophet::Node* n)
{
    iterator i;
    ASSERT(n != NULL);
    if (find(n->dest_id(), i))
    {
        // remove from list and from ProphetStorage
        ProphetNode* a = static_cast<ProphetNode*>(*i);
        list_.erase(i);
        ProphetStore::instance()->del(a);
        delete a;
    }
}

const prophet::Node*
ProphetNodeList::find(const std::string& dest_id) const
{
    iterator i;
    ProphetNodeList* me = const_cast<ProphetNodeList*>(this);
    if (me == NULL) return NULL;
    if (me->find(dest_id,i))
        return *i;
    return NULL;
}

void
ProphetNodeList::clone(prophet::Table* nodes,
                       const prophet::NodeParams* params)
{
    if (nodes == NULL || params == NULL) return;

    std::list<const prophet::Node*> list(list_.begin(),list_.end());
    nodes->assign(list,params);
}

void
ProphetNodeList::clear()
{
    while (!list_.empty())
    {
        delete list_.front();
        list_.pop_front();
    }
}

bool
ProphetNodeList::find(const std::string& dest_id, iterator& i)
{
    i = list_.begin();
    while (i != list_.end() && (*i)->dest_id() < dest_id)
        i++;
    return (i != list_.end() && (*i)->dest_id() == dest_id);
}

}; // namespace dtn

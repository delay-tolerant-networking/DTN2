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
#include <dtn-config.h>
#endif
#include "ProphetLinkList.h"

namespace dtn
{

LinkRef
ProphetLinkList::NULL_LINK("ProphetLinkList");

ProphetLinkList::ProphetLinkList()
{}

ProphetLinkList::~ProphetLinkList()
{
    clear();
}

void
ProphetLinkList::add(const LinkRef& l)
{
    iterator i;
    if (! find(l->remote_eid().c_str(),i))
    {
        ProphetLink* lr = new ProphetLink(l);
        list_.insert(i,lr);
    }
}

void
ProphetLinkList::del(const LinkRef& l)
{
    iterator i;
    if (find(l->remote_eid().c_str(),i))
    {
        delete *i;
        list_.erase(i);
    }
}

const prophet::Link*
ProphetLinkList::find(const char* remote_eid) const
{
    iterator i;
    ProphetLinkList* me = const_cast<ProphetLinkList*>(this);
    if (me != NULL && me->find(remote_eid,i))
        return (*i);
    return NULL;
}

const LinkRef&
ProphetLinkList::find_ref(const prophet::Link* link) const
{
    if (link == NULL) return NULL_LINK;
    return find_ref(link->remote_eid());
}

const LinkRef&
ProphetLinkList::find_ref(const char* remote_eid) const
{
    iterator i;
    ProphetLinkList* me = const_cast<ProphetLinkList*>(this);
    if (me != NULL && me->find(remote_eid,i))
        return (*i)->ref();
    return NULL_LINK;
}

void
ProphetLinkList::clear()
{
    while (!list_.empty())
    {
        delete list_.front();
        list_.pop_front();
    }
}

bool
ProphetLinkList::find(const char* remote_eid, iterator& i)
{
    i = list_.begin();
    while (i != list_.end() &&
            (strncmp((*i)->remote_eid(),remote_eid,strlen((*i)->remote_eid())) < 0) )
        i++;
    if (i == list_.end()) return false;
    return strncmp((*i)->remote_eid(),remote_eid,strlen((*i)->remote_eid())) == 0;
}

}; // namespace dtn

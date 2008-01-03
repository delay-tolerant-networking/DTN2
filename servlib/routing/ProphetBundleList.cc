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
#include "ProphetBundleList.h"

namespace dtn
{

BundleRef
ProphetBundleList::NULL_BUNDLE("ProphetBundleList");

ProphetBundleList::ProphetBundleList(prophet::Repository::BundleCoreRep* core)
    : list_(core) {}

ProphetBundleList::~ProphetBundleList()
{
    clear();
}

void
ProphetBundleList::add(const BundleRef& b)
{
    ProphetBundle* pb = new ProphetBundle(b);
    if (!list_.add(pb))
        delete pb;
}

void
ProphetBundleList::add(const prophet::Bundle* b)
{
    if (list_.add(b))
        return;

    prophet::Bundle* pb = const_cast<prophet::Bundle*>(b);
    delete pb;
}

void
ProphetBundleList::del(const BundleRef& b)
{
    const_iterator i;
    if (find(b->dest().str(),
             b->creation_ts().seconds_,
             b->creation_ts().seqno_, i))
    {
        prophet::Bundle* bundle = const_cast<prophet::Bundle*>(*i);
        list_.del(*i);
        delete bundle;
    }
}

void
ProphetBundleList::del(const prophet::Bundle* b)
{
    prophet::Bundle* bundle = const_cast<prophet::Bundle*>(b);
    list_.del(bundle);
    delete bundle;
}

const prophet::Bundle*
ProphetBundleList::find(const std::string& dst,
                        u_int creation_ts,
                        u_int seqno) const
{
    const_iterator i;
    if (find(dst,creation_ts,seqno,i))
        return *i;
    return NULL;
}

const BundleRef&
ProphetBundleList::find_ref(const prophet::Bundle* b) const
{
    if (b != NULL)
    {
        const_iterator i;
        if (find(b->destination_id(),
                 b->creation_ts(),
                 b->sequence_num(),i))
            return dynamic_cast<const ProphetBundle*>(*i)->ref();
    }
    return NULL_BUNDLE;
}

void
ProphetBundleList::clear()
{
    while (!list_.empty())
    {
        prophet::Bundle* b = const_cast<prophet::Bundle*>(
                list_.get_bundles().front());
        list_.del(b);
        // clean up memory from Facade wrapper
        delete b;
    }
}

bool
ProphetBundleList::find(const std::string& dst,
        u_int creation_ts, u_int seqno, const_iterator& i) const
{
    i = list_.get_bundles().begin();
    while (i != list_.get_bundles().end())
    {
        if ((*i)->creation_ts() == creation_ts &&
            (*i)->sequence_num() == seqno &&
            (*i)->destination_id() == dst) break;
        i++;
    }

    if (i == list_.get_bundles().end()) return false;

    return ((*i)->creation_ts() == creation_ts &&
            (*i)->sequence_num() == seqno &&
            (*i)->destination_id() == dst);
}

}; // namespace dtn

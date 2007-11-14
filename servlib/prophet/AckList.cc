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

#include <time.h>
#include "BundleCore.h"
#include "AckList.h"

namespace prophet
{

AckList::~AckList()
{
    for (palist::iterator i = acks_.begin(); i != acks_.end(); i++)
    {
        delete (*i);
    }
    acks_.clear();
}

bool
AckList::insert(const std::string& dest_id, u_int32_t cts,
                u_int32_t seq, u_int32_t ets)
{
    // default to 1 day
    if (ets == 0) ets = 86400;

    Ack a(dest_id,cts,seq,ets); 
    return insert(&a);
}

bool
AckList::insert(const Bundle* b, const BundleCore* core)
{
    if (b == NULL || core == NULL) return false;

    return insert(core->get_route(b->destination_id()),
                  b->creation_ts(),
                  b->sequence_num(),
                  b->expiration_ts());
}

bool
AckList::insert(const Ack* a)
{
    if (a == NULL) return false;

    Ack* na = new Ack(*a);

    // attempt to insert
    if (acks_.insert(na).second)
        return true;

    // failed, so free up memory
    delete na;
    return false;
}

size_t
AckList::clone(PointerList<Ack>& list) const
{
    size_t num = 0;
    // clear the incoming
    list.clear();
    // walk the internal, visiting each Ack
    for (palist::const_iterator i = acks_.begin(); i != acks_.end(); i++)
    {
        // give away a copy of each visited Ack
        list.push_back(new Ack(**i));
        // ... counting as we go
        num++;
    }
    // inform caller of how many elements
    return num;
}

size_t
AckList::fetch(const std::string& dest_id, PointerList<Ack>* list) const
{
    size_t num = 0;

    // clear incoming, if set
    if (list != NULL) list->clear();

    // create a search parameter of palist::key_type
    Ack a(dest_id);

    // find the beginning of the sequence that matches dest_id
    palist::const_iterator i = acks_.lower_bound(&a);

    // walk along until either the string no longer matches or
    // we fall off the end of the sequence
    while (i != acks_.end() && (*i)->dest_id().compare(dest_id) == 0)
    {
        // if set, give the list pointer a copy of each Ack we find
        if (list != NULL) list->push_back(new Ack(**(i++)));
        num++;
    }

    // inform the caller of how many were found
    return num;
}

size_t
AckList::expire()
{
    size_t num = 0;

    // grab current epoch seconds
    time_t now = time(0);

    // walk the list of Acks, careful how we increment since
    // the erase() operation screws up iterator state
    for (palist::iterator i = acks_.begin(); i != acks_.end(); )
    {
        Ack* a = *i;
        // test for expired condition
        if (now - a->cts() > a->ets())
        {
            num++;
            // post increment to sidestep iterator screwiness
            acks_.erase(i++);
            // clear up memory 
            delete a;
        }
        // not expired, increment past this one
        else i++;
    }
    // inform caller of how many were removed
    return num;
}

bool
AckList::is_ackd(const std::string& dest_id,
                 u_int32_t cts, u_int32_t seq) const
{
    // create search parameter of type palist::key_type
    Ack a(dest_id);

    // initialize iterator to beginning of matching sequence
    palist::iterator i = acks_.lower_bound(&a);

    // walk the sequence until no more matches, or end of sequence
    while (i != acks_.end())
    {
        // test for no-more-matches
        if ((*i)->dest_id().compare(dest_id) != 0)
            break;
        // all parameters match?
        if ((*i)->cts() == cts && (*i)->seq() == seq)
            return true;
        i++;
    }
    // not found, therefore must not be Ack'd
    return false;
}

}; // namespace prophet

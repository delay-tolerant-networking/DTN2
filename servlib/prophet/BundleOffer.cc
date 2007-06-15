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

#include "BundleOffer.h"

namespace prophet
{

#define LOG(_args...) core_->print_log("offer",BundleCore::LOG_DEBUG,_args)

BundleOffer::BundleOffer(BundleCore* core,
                         const BundleList& bundles,
                         FwdStrategyComp* comp,
                         Decider* decider)
    : core_(core), comp_(comp), decider_(decider)
{
    for (BundleList::const_iterator i = bundles.begin();
            i != bundles.end(); i++)
        add_bundle(*i);
}

BundleOffer::~BundleOffer()
{
    delete comp_;
    delete decider_;
}

void
BundleOffer::add_bundle(const Bundle* b)
{
    if (b == NULL) return;

    if ((*decider_)(b))
    {
        // insert in (reverse) sorted order
        List::iterator i = list_.begin();
        while (i != list_.end() && (*comp_)(b,*i))
            i++;
        list_.insert(i,b);
        LOG("offering %s %u %u",b->destination_id().c_str(),
                b->creation_ts(), b->sequence_num());
    }
    else
    {
        LOG("not offering %s %u %u",b->destination_id().c_str(),
                b->creation_ts(), b->sequence_num());
    }
}

const BundleOfferList&
BundleOffer::get_bundle_offer(const Dictionary& ribd,
                              const AckList* acks)
{
    bolist_.clear();
    List::iterator i = list_.begin();
    while (i != list_.end())
    {
        std::string eid = core_->get_route((*i)->destination_id());
        u_int16_t sid = ribd.find(eid);
        if (sid != Dictionary::INVALID_SID)
            bolist_.add_offer(*i,sid);
        i++;
    }
    if (acks != NULL && !acks->empty())
    {
        PointerList<Ack> list;
        acks->clone(list);
        for (PointerList<Ack>::iterator ai = list.begin(); ai != list.end(); ai++)
        {
            std::string eid = core_->get_route((*ai)->dest_id());
            u_int16_t sid = ribd.find(eid);
            if (sid != Dictionary::INVALID_SID)
                bolist_.add_offer((*ai)->cts(),(*ai)->seq(),sid,false,true);
        }
    }
    return bolist_;
}

}; // namespace prophet

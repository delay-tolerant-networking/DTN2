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

#ifndef _PROPHET_TLV_CREATOR_H_
#define _PROPHET_TLV_CREATOR_H_

#include "Table.h"
#include "BundleCore.h"
#include "BundleList.h"
#include "Dictionary.h"
#include "RIBDTLV.h"
#include "RIBTLV.h"
#include "OfferTLV.h"
#include "BundleTLVEntryList.h"
#include "BundleOffer.h"
#include "Oracle.h"
#include <string>

namespace prophet
{

struct TLVCreator
{
    static RIBDTLV* ribd(BundleCore* core,
                         const Table* nodes,
                         const std::string& sender,
                         const std::string& receiver)
    {
        // reject the oddball
        if (core == NULL || nodes == NULL) return NULL;

        Dictionary ribd(sender,receiver);
        for (Table::const_iterator i = nodes->begin(); i != nodes->end(); i++)
        {
            // grab the route from the iterator
            std::string dest = i->first;
            std::string eid = core->get_route(dest);
            // attempt to insert, fail out if unsuccessful
            if (ribd.insert(eid) == Dictionary::INVALID_SID &&
                ribd.find(eid) == Dictionary::INVALID_SID) return NULL;
        }

        ribd.dump(core,__FILE__,__LINE__);
        return new RIBDTLV(ribd);
    }

    static RIBTLV* rib(Oracle* oracle,
                       const Dictionary& ribd,
                       bool relay_node,
                       bool accept_custody,
                       bool internet_gw = false)
    {
        // reject the oddball
        if (oracle == NULL) return NULL;

        const Table* nodes = oracle->nodes();
        std::string sender = ribd.find(0);
        std::string receiver = ribd.find(1);

        RIBNodeList list;
        for (Table::const_iterator i = nodes->begin(); i != nodes->end(); i++)
        {
            const std::string eid = i->first;
            if (eid == sender || eid == receiver) continue;
            const Node* n = i->second;
            u_int16_t sid = ribd.find(eid);
            if (sid == Dictionary::INVALID_SID)
                return NULL; // log error?
            oracle->core()->print_log("rib",BundleCore::LOG_DEBUG,
                    "%s (%u) -> %.2f",eid.c_str(),sid,n->p_value());
            list.push_back(new RIBNode(n,sid));
        }

        ribd.dump(oracle->core(),__FILE__,__LINE__);
        return new RIBTLV(list,relay_node,accept_custody,internet_gw);
    }

    static OfferTLV* offer(Oracle* oracle,
                           const Link* nexthop,
                           const Dictionary& ribd,
                           const Table& remote)
    {
        // reject the oddball
        if (oracle == NULL) return NULL;

        // create comp
        FwdStrategyComp* comp = FwdStrategy::strategy(
                oracle->params()->fs(),
                oracle->nodes(),
                &remote);

        // create decider
        Decider* d = Decider::decider(
                oracle->params()->fs(),
                nexthop,
                oracle->core(),
                oracle->nodes(),
                &remote,
                oracle->stats(),
                oracle->params()->max_forward(),
                oracle->params()->relay_node());

        // offer's destructor will clean up comp and d
        BundleOffer offer(oracle->core(),oracle->core()->bundles(),comp,d);

        // Create a reduced list of ACKs by only tacking on those that 
        // haven't already been sent

        PointerList<Ack> acklist; // list of clones from oracle's ACKs
        AckList acks; // reduced list to send to get_bundle_offer
        AckList* link_acks = const_cast<Link*>(nexthop)->acks();

        oracle->acks()->clone(acklist);
        for(PointerList<Ack>::iterator i = acklist.begin();
                i != acklist.end(); i++)
        {
            // returns false if already exists in list (meaning, already sent)
            if (link_acks->insert(*i))
                acks.insert(*i);
        }

        BundleOfferList list = offer.get_bundle_offer(ribd,&acks);
        
        if (list.empty())
            oracle->core()->print_log("offer",BundleCore::LOG_DEBUG,
                    "empty bundle offer");
        else
        {
            for (BundleOfferList::const_iterator i = list.begin(); 
                    i != list.end(); i++)
            {
                oracle->core()->print_log("offer",BundleCore::LOG_DEBUG,
                        "%u %u %u %s%s%s",
                        (*i)->creation_ts(),
                        (*i)->seqno(),
                        (*i)->sid(),
                        (*i)->custody() ? "C" : "-",
                        (*i)->accept() ? "A" : "-",
                        (*i)->ack() ? "K" : "-");
            }
        }

        return new OfferTLV(list);
    }

    static ResponseTLV* response(Oracle* oracle,
                                 const BundleOfferList& offers,
                                 BundleResponseList& list,
                                 const Dictionary& ribd)
    {
        // reject the oddball
        if (oracle == NULL) return NULL;

        ribd.dump(oracle->core(),__FILE__,__LINE__);

        for (BundleOfferList::const_iterator i = offers.begin();
                i != offers.end(); i++)
        {
            // pull out the three-way tuple that uniquely identifies bundles
            u_int32_t cts = (*i)->creation_ts();
            u_int32_t seq = (*i)->seqno();
            u_int16_t sid = (*i)->sid();
            std::string eid = ribd.find(sid);

            // First delete any ACK'd bundles, and store the ACK
            const Bundle* b = oracle->core()->find(
                    oracle->core()->bundles(),eid,cts,seq);
            if ((*i)->ack())
            {
                oracle->core()->print_log("response",BundleCore::LOG_DEBUG,
                        "ACK %s %u:%u",eid.c_str(),cts,seq);

                if (b != NULL)
                    oracle->ack(b);
                else
                    oracle->acks()->insert(eid,cts,seq);

                // also remember that it came from this link, so as not 
                // to propagate this ACK over this link again

            }
            else
            // only request the bundle if not already present in host storage
            // and if no ACK exists for the bundle
            if (b == NULL)
            {
                // no need to request bundles that have already been delivered
                if (oracle->acks()->is_ackd(eid,cts,seq))
                {
                    oracle->core()->print_log("response",BundleCore::LOG_DEBUG,
                        "not requesting ACK'd bundle: %s %u %u",
                        eid.c_str(),cts,seq);
                    continue;
                }

                // Logically AND local settings for custody accept with
                // remote's request for custody transfer
                bool custody = (*i)->custody() &&
                               oracle->core()->custody_accepted();
                if (list.add_response(cts,seq,sid,custody))
                {
                    const BundleResponseEntry* bre = list.back();
                    oracle->core()->print_log("response",BundleCore::LOG_DEBUG,
                        "%s (%u) %u %u",eid.c_str(),bre->sid(),
                        bre->creation_ts(),bre->seqno());
                }
            }
        }

        return new ResponseTLV(list);
    }

}; // struct TLVCreator

}; // namespace prophet

#endif // _PROPHET_TLV_CREATOR_H_

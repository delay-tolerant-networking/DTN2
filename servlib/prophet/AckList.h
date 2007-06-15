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

#ifndef _PROPHET_ACK_LIST_H_
#define _PROPHET_ACK_LIST_H_

#include <set>
#include "Ack.h"
#include "Bundle.h"
#include "PointerList.h"

namespace prophet
{

struct AckComp : public std::less<Ack*>
{
    bool operator() (const Ack* a, const Ack* b) const
    {
        return *a < *b;
    }
}; // AckComp

// forward declaration
class BundleCore;

/**
 * Section 3.5 (p. 16) describes Prophet ACKs as needing to persist
 * in a node's storage beyond the lifetime of the bundle they represent.
 * ProphetAckList is that persistence (but not [yet] serializable to
 * permanent storage).
 */
class AckList
{
public:
    /**
     * Default constructor
     */
    AckList() {}

    /**
     * Copy constructor
     */
    AckList(const AckList& list)
        : acks_(list.acks_) {}

    /**
     * Destructor
     */
    ~AckList();

    /**
     * Convenience method for inserting Ack into list; return true
     * upon success, else false if Ack already exists
     *
     * Expiration time stamp is actually an offset in seconds, from
     * creation time. Default is 0 (use offset of one day, 86400 sec)
     */
    bool insert(const std::string& dest_id, u_int32_t cts,
                u_int32_t seq = 0, u_int32_t ets = 0);

    /**
     * Convenience method for inserting Ack into list; return true
     * upon success, else false if Ack already exists
     */
    bool insert(const Bundle* b, const BundleCore* core);

    /**
     * Insert Ack, return true on success, else false if Ack exists
     */
    bool insert(const Ack* ack);

    /**
     * Export list of Acks to PointerList<Ack>, return number of
     * elements exported
     */
    size_t clone(PointerList<Ack>& list) const;

    /**
     * Given a destination ID, return the number of Acks that match
     * (exact match only, no pattern matches). If list is non NULL,
     * fill with Acks that match
     */
    size_t fetch(const std::string& dest_id, PointerList<Ack>* list) const;

    /**
     * Visit every ACK in the list, and delete those for which the
     * expiration has passed; return the number of elements deleted
     */
    size_t expire();

    /**
     * Number of elements currently in list
     */
    size_t size() const { return acks_.size(); }
    
    /**
     * Convenience function to answer the question of whether this 
     * Bundle has been Ack'd
     */
    bool is_ackd(const std::string& dest_id,
                 u_int32_t cts, u_int32_t seq) const;

    /**
     * Accessor
     */
    bool empty() const { return acks_.empty(); }

protected:
    typedef std::set<Ack*,AckComp> palist;
    palist acks_;

}; // AckList

}; // namespace prophet

#endif // _PROPHET_ACK_LIST_H_

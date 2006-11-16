/*
 *    Copyright 2006 Baylor University
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

#include "ProphetLists.h"

namespace dtn {

/**
 * Given a route ID and creation timestamp,
 * find and return a Bundle with matching dest ID
 */
Bundle*
BundleVector::find(const EndpointID& dest,
                   u_int32_t creation_ts) const
{
    EndpointIDPattern route = Prophet::eid_to_route(dest);
    for(const_iterator i = begin();
        i != end();
        i++)
    {
        Bundle* b = *i;
        if (b->creation_ts_.seconds_ == creation_ts &&
            route.match(b->dest_))
        {
            return b;
        }
    }
    return NULL;
}

ProphetTable::ProphetTable()
    : lock_(new oasys::SpinLock())
{
    table_.clear();
}

ProphetNode*
ProphetTable::find(const EndpointID& eid) const
{
    ASSERT( eid.equals(EndpointID::NULL_EID()) == false );
    EndpointID routeid = Prophet::eid_to_routeid(eid);

    oasys::ScopeLock(lock_,"ProphetTable::find");
    rib_table::const_iterator it =
        (rib_table::const_iterator) table_.find(routeid);
    if(it != table_.end()) {
        return (ProphetNode*) (*it).second;
    }
    return NULL;
}

double
ProphetTable::p_value(const EndpointID& eid) const
{
    ProphetNode *n = find(eid);
    if( n == NULL )
        return 0.0;

    return n->p_value();
}

void
ProphetTable::update(ProphetNode* node)
{
    EndpointID eid(node->remote_eid());
    ASSERT( eid.equals(EndpointID::NULL_EID()) == false );

    oasys::ScopeLock(lock_,"ProphetTable::update");

    // grab an iterator to insertion point in rib_table
    rib_table::iterator lb = table_.lower_bound(eid);

    // if this is an update to an existing key
    if ( lb != table_.end() &&
         !(table_.key_comp()(eid,lb->first)) &&
         !(table_.key_comp()(lb->first,eid)) )
    {
        ProphetNode* old = lb->second;
        lb->second = node;
        if (node != old) delete old;
    }
    // otherwise shove it in, right here
    else {
        table_.insert(lb,rib_table::value_type(eid,node));
    }
}

size_t
ProphetTable::dump_table(ProphetNodeList& list) const
{ 
    oasys::ScopeLock(lock_,"ProphetTable::dump_table");
    size_t num = 0;
    for(rib_table::const_iterator rti =  table_.begin();
                                  rti != table_.end();
                                  rti++) {
        ProphetNode* node = new ProphetNode(*((*rti).second));
        list.push_back(node);
        num++;
    }
    return num;
}

ProphetTable::iterator
ProphetTable::begin()
{ 
    ASSERT( lock_->is_locked_by_me() );
    return table_.begin();
}

ProphetTable::iterator
ProphetTable::end()
{ 
    ASSERT( lock_->is_locked_by_me() );
    return table_.end();
}

void
ProphetTable::truncate(double epsilon)
{
    oasys::ScopeLock(lock_,"ProphetTable::truncate");
    for(iterator i = table_.begin();
        i != table_.end();
        i++)
    {
        ProphetNode* node = (*i).second;
        if (node->p_value() < epsilon)
        {
            table_.erase(i);
            delete node;
        }
    }
}

void
ProphetTableAgeTimer::timeout(const struct timeval& now)
{
    (void)now;
    int c = 0;
    oasys::ScopeLock(table_->lock(),"ProphetTableAgeTimer");
    ProphetTable::iterator i = table_->begin();
    while(i != table_->end()) {
        ProphetNode* node = (*i).second;
        node->update_age();
        i++; c++;
    }
    table_->truncate(epsilon_);
    reschedule();
    log_debug("aged %d prophet nodes",c);
}

void
ProphetAckAgeTimer::timeout(const struct timeval& now)
{
    (void)now;
    list_->expire(); // no arg means, use current time
    reschedule();
}

ProphetDictionary::ProphetDictionary(const EndpointID& sender,
                                     const EndpointID& receiver)
    : guess_(0)
{
    clear();
    // By definition, sender of SYN is 0 and sender of SYNACK is 1
    if (sender.equals(EndpointID::NULL_EID()) == false)
    {
        ribd_[sender] = 0; // by definition
        rribd_[0] = sender;
    }
    if (receiver.equals(EndpointID::NULL_EID()) == false)
    {
        ribd_[receiver] = 1; // by definition
        rribd_[1] = receiver;
    }
    // neither sender nor receiver gets encoded into RIBDTLV
    // so no call to update_guess
}

ProphetDictionary::ProphetDictionary(const ProphetDictionary& pd)
    : ribd_(pd.ribd_), rribd_(pd.rribd_), guess_(pd.guess_)
{
}

bool
ProphetDictionary::is_assigned(const EndpointID& eid) const
{
    ribd::const_iterator it =
        (ribd::const_iterator) ribd_.find(eid);
    if (it != ribd_.end()) {
        return true;
    }
    return false;
}

u_int16_t
ProphetDictionary::find(const EndpointID& eid) const
{
    ribd::const_iterator it =
        (ribd::const_iterator) ribd_.find(eid);
    if (it != ribd_.end()) {
        return (*it).second;
    }
    return 0;
}

EndpointID
ProphetDictionary::find(u_int16_t id) const
{
    rribd::const_iterator it = (rribd::const_iterator) rribd_.find(id);
    if (it != rribd_.end()) {
        return (*it).second;
    }
    return EndpointID::NULL_EID();
}

u_int16_t
ProphetDictionary::insert(const EndpointID& eid)
{
    u_int16_t sid = ribd_.size();
    bool res = assign(eid,sid);
    return res ? sid : 0;
}

bool
ProphetDictionary::assign(const EndpointID& eid, u_int16_t sid)
{
    // validate internal state
    ASSERT(ribd_.size() == rribd_.size());
    if (ribd_.size() >= 2) 
    {
        EndpointID sender = rribd_[0];
        EndpointID receiver = rribd_[1];
        ASSERT(eid.equals(sender) && sid == 0);
        ASSERT(eid.equals(receiver) && sid == 1);
    }

    // first attempt to insert into forward lookup
    bool res = ribd_.insert(
                            std::pair<EndpointID,u_int16_t>(eid,sid)
                            ).second;
    if ( ! res )
        return false;
    // next attempt to insert into reverse lookup
    res = rribd_.insert(std::pair<u_int16_t,EndpointID>(sid,eid)).second;
    if ( ! res ) {
        ribd_.erase(eid);
    } else {
        // update on success
        // skip sender/receiver peer endpoints
        if ( ! (sid == 0 || sid == 1))
        {
            update_guess(eid.str().size());
        }
    }
    ASSERT(ribd_.size() == rribd_.size());
    return res;
}

void
ProphetDictionary::clear() {
    // get rid of the old dictionary
    ribd_.clear();
    rribd_.clear();
    guess_ = 0;
}

void
BundleOfferList::sort(ProphetDictionary* ribd,
                      ProphetTable* nodes,
                      u_int16_t sid)
{
    oasys::ScopeLock l(lock_,"BundleOfferList::prioritize");
    std::vector<BundleOffer*> list(list_.begin(),list_.end());
    std::sort(list.begin(),list.end(),BundleOfferComp(ribd,nodes));
    list_.clear();
    // move any bundles destined for local node to front of list
    for(std::vector<BundleOffer*>::iterator i = list.begin();
        i != list.end();
        i++)
    {
        BundleOffer* bo = (*i);
        if (bo->sid() == sid) 
        {
            list_.push_front(bo);
        }
        else
        {
            list_.push_back(bo);
        }
    }
}

bool
BundleOfferList::remove_bundle(u_int32_t cts, u_int16_t sid)
{
    oasys::ScopeLock l(lock_,"BundleOfferList::remove_bundle");
    BundleOffer* retval = NULL;
    for (iterator i = list_.begin(); 
         i != list_.end();
         i++)
    {
        if ((*i)->creation_ts() == cts && (*i)->sid() == sid) {
            retval = *i;
            list_.erase(i);
            delete retval;
            return true;
        }
    }
    return false;
}

size_t 
BundleOfferList::size() const
{
    return list_.size();
}

bool
BundleOfferList::empty() const
{
    return list_.empty();
}

void
BundleOfferList::clear()
{
    oasys::ScopeLock l(lock_,"BundleOfferList::clear");
    list_.clear();
}

void
BundleOfferList::push_back(BundleOffer* bo)
{
    oasys::ScopeLock l(lock_,"BundleOfferList::push_back");
    list_.push_back(bo);
}

void
BundleOfferList::add_offer(BundleOffer* offer)
{
    oasys::ScopeLock l(lock_,"BundleOfferList::add_offer");
    ASSERT(type_ != BundleOffer::UNDEFINED);
    ASSERT(offer->type() == type_);
    list_.push_back(new BundleOffer(*offer));
}

void
BundleOfferList::add_offer(u_int32_t cts, u_int16_t sid,
        bool custody, bool accept, bool ack)
{
    oasys::ScopeLock l(lock_,"BundleOfferList::add_offer");
    ASSERT(type_ != BundleOffer::UNDEFINED);
    list_.push_back(new BundleOffer(cts, sid, custody, accept, ack, type_));
}

void
BundleOfferList::add_offer(Bundle* b,u_int16_t sid)
{
    oasys::ScopeLock l(lock_,"BundleOfferList::add_offer");
    ASSERT(type_ != BundleOffer::UNDEFINED);
    list_.push_back(new BundleOffer(b->creation_ts_.seconds_, sid,
                              b->custody_requested_, false, false, type_));
}

BundleOffer*
BundleOfferList::find(u_int32_t cts, u_int16_t sid) const
{
    oasys::ScopeLock l(lock_,"BundleOfferList::find");
    BundleOffer* retval = NULL;
    for (const_iterator i = list_.begin(); 
         i != list_.end();
         i++)
    {
        if ((*i)->creation_ts() == cts && (*i)->sid() == sid) {
            retval = *i;
            break;
        }
    }
    return retval;
}

BundleOfferList::const_iterator
BundleOfferList::begin() const
{
    ASSERT(lock_->is_locked_by_me());
    return (const_iterator) list_.begin();
}

BundleOfferList::const_iterator
BundleOfferList::end() const
{
    ASSERT(lock_->is_locked_by_me());
    return (const_iterator) list_.end();
}

BundleOfferList::iterator
BundleOfferList::begin()
{
    ASSERT(lock_->is_locked_by_me());
    return list_.begin();
}

BundleOfferList::iterator
BundleOfferList::end()
{
    ASSERT(lock_->is_locked_by_me());
    return list_.end();
}

ProphetAckList::ProphetAckList()
    : lock_(new oasys::SpinLock())
{
    acks_.clear();
}

ProphetAckList::~ProphetAckList()
{
    {
        oasys::ScopeLock(lock_,"ProphetAckList::destructor");
        palist::iterator iter;
        while(!acks_.empty()) {
            ProphetAck* a;
            iter = acks_.begin();
            a = *iter;
            acks_.erase(a);
            delete a;
        }
    }
    acks_.clear();
    delete lock_;
}

size_t
ProphetAckList::count(const EndpointID& eid) const
{
    oasys::ScopeLock(lock_,"ProphetAckList::count");
    size_t retval = 0;
    ProphetAck p(eid);
    // cts_ defaults to 0
    // non-end() should point to the first matching ACK
    palist::iterator iter = acks_.lower_bound(&p);
    while( iter != acks_.end() ) {
        if ( !(*iter)->dest_id_.equals(eid) )
            break;
        retval++;
        iter++;
    }
    return retval;
}

bool
ProphetAckList::insert(const EndpointID& eid, u_int32_t cts, u_int32_t ets)
{
    oasys::ScopeLock(lock_,"ProphetAckList::insert");
    if (ets == 0)
        ets = cts + 86400;  // keep ACK for one day unless spec'd otherwise
    ProphetAck* p = new ProphetAck(eid,cts,ets);
    if (acks_.insert(p).second)
        return true;
    delete p;
    return false;
}

bool
ProphetAckList::insert(ProphetAck* p)
{
    oasys::ScopeLock(lock_,"ProphetAckList::insert");
    return acks_.insert(p).second;
}

size_t
ProphetAckList::expire(u_int32_t older_than)
{
    oasys::ScopeLock(lock_,"ProphetAckList::expire");
    oasys::Time now(older_than);
    size_t how_many = 0;
    if(older_than == 0)
        now.get_time();
    palist::iterator iter = acks_.begin();
    while(iter != acks_.end()) {
        ProphetAck* p = *iter;
        if (p->ets_ < (unsigned int) now.sec_) {
            how_many++;
            acks_.erase(iter);
            delete p;
            iter = acks_.begin(); // erase() invalidates, we get to start again
        } else
            iter++;
    }
    return how_many;
}

size_t
ProphetAckList::fetch(const EndpointIDPattern& eid,
                      PointerList<ProphetAck>& list) const
{
    oasys::ScopeLock(lock_,"ProphetAckList::fetch");
    size_t retval = 0;
    palist::iterator iter = acks_.begin();
    while( iter != acks_.end() ) {
        if (eid.match((*iter)->dest_id_)) {
            list.push_back(new ProphetAck((*(*iter))));
            retval++;
        }
        iter++;
    }
    return retval;
}

bool
ProphetAckList::is_ackd(const EndpointID& eid, u_int32_t cts) const
{
    oasys::ScopeLock(lock_,"ProphetAckList::fetch");
    ProphetAck p(eid);
    palist::iterator iter = acks_.lower_bound(&p);
    while (iter != acks_.end())
    {
        if (!(*iter)->dest_id_.equals(eid))
            break;
        if ((*iter)->cts_ == cts) 
            return true;
    }
    return false;
}

ProphetStats::~ProphetStats()
{
    // For each member of pstats, delete the new'd memory
    {
        oasys::ScopeLock(lock_,"ProphetStats::destructor");
        iterator i = pstats_.begin();
        while( i != pstats_.end() ) {
            ProphetStatsEntry* pse = (*i).second;
            delete pse;
            i++;
        }
    }

    pstats_.clear();
    delete lock_;
}

ProphetStatsEntry*
ProphetStats::find_entry(const Bundle* b)
{
    oasys::ScopeLock(lock_,"ProphetStats::find_entry");
    u_int32_t id = b->bundleid_;
    ProphetStatsEntry* pse = NULL;
    const_iterator it = (const_iterator) pstats_.find(id);
    if (it != pstats_.end())
        pse = (*it).second;
    else { 
        pse = new ProphetStatsEntry();
        memset(pse,0,sizeof(ProphetStatsEntry));
        pstats_[id] = pse;
    }
    return pse;
}

void
ProphetStats::update_stats(const Bundle* b, double p)
{
    oasys::ScopeLock(lock_,"ProphetStats::update_stats");
    ProphetStatsEntry* pse = find_entry(b);

    ASSERT(pse != NULL);

    if (pse->p_max_ < p) {
        pse->p_max_ = p;
    }

    // Section 3.7, equation 7
    pse->mopr_ += (1 - pse->mopr_) * p;

    // Section 3.7, equation 8
    pse->lmopr_ += p;

    pstats_[b->bundleid_] = pse;
}

double
ProphetStats::get_p_max(const Bundle* b)
{
    oasys::ScopeLock(lock_,"ProphetStats::get_p_max");
    ProphetStatsEntry* pse = find_entry(b);
    return pse->p_max_;
}

double
ProphetStats::get_mopr(const Bundle* b)
{
    oasys::ScopeLock(lock_,"ProphetStats::get_mopr");
    ProphetStatsEntry* pse = find_entry(b);
    return pse->mopr_;
}

double
ProphetStats::get_lmopr(const Bundle* b)
{
    oasys::ScopeLock(lock_,"ProphetStats::get_lmopr");
    ProphetStatsEntry* pse = find_entry(b);
    return pse->lmopr_;
}

void
ProphetStats::drop_bundle(const Bundle* b)
{
    oasys::ScopeLock(lock_,"ProphetStats::drop_bundle");
    ProphetStatsEntry* pse = NULL;
    iterator it = pstats_.find(b->bundleid_);
    if (it != pstats_.end())
        pse = (*it).second;
    delete pse;
    pstats_.erase(it);
    dropped_++;
}

} // namespace dtn

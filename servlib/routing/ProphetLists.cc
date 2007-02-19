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

#include <oasys/util/Time.h>
#include "ProphetTLV.h"

#include "ProphetLists.h"
#include "storage/ProphetStore.h"

namespace dtn {

const LinkRef ProphetDecider::null_link_("ProphetDecider null");
const u_int16_t ProphetDictionary::INVALID_SID = 0xffff;
	
ProphetTable::ProphetTable(bool store)
    : lock_(new oasys::SpinLock()),
      store_(store)
{
    table_.clear();
}

ProphetTable::~ProphetTable()
{
    oasys::ScopeLock l(lock_,"destructor");
    clear();
    l.unlock();
    delete lock_;
}

ProphetNode*
ProphetTable::find(const EndpointID& eid) const
{
    ASSERT( eid.equals(EndpointID::NULL_EID()) == false );
    EndpointID routeid = Prophet::eid_to_routeid(eid);

    oasys::ScopeLock l(lock_,"ProphetTable::find");
    rib_table::const_iterator it =
        (rib_table::const_iterator) table_.find(routeid);
    if(it != table_.end())
    {
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
ProphetTable::update(ProphetNode* node, bool add_to_store)
{
    ASSERT( node != NULL );
    ASSERT( node->params() != NULL );
    EndpointID eid(node->remote_eid());
    ASSERT( eid.equals(EndpointID::NULL_EID()) == false );

    oasys::ScopeLock l(lock_,"ProphetTable::update");

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
        if (add_to_store) store_update(node);
    }
    // otherwise shove it in, right here
    else
    {
        table_.insert(lb,rib_table::value_type(eid,node));
        if (add_to_store) store_add(node);
    }
}

void
ProphetTable::store_add(ProphetNode* node)
{
    if (store_)
    {
        if (!ProphetStore::instance()->add(node))
        {
            log_crit_p("/dtn/route/prophet/table",
                       "error inserting node for %s into data store!",
                       node->remote_eid().c_str());
        }
    }
}

void
ProphetTable::store_update(ProphetNode* node)
{
    if (store_)
    {
        if (!ProphetStore::instance()->update(node))
        {
            log_crit_p("/dtn/route/prophet/table",
                       "error updating node for %s in data store!",
                       node->remote_eid().c_str());
        }
    }
}

void
ProphetTable::store_del(ProphetNode* node)
{
    if (store_)
    {
        if (!ProphetStore::instance()->del(node))
        {
            log_crit_p("/dtn/route/prophet/table",
                       "error removing node for %s from data store",
                       node->remote_eid().c_str());
        }
    }
}

size_t
ProphetTable::dump_table(ProphetNodeList& list) const
{ 
    oasys::ScopeLock l(lock_,"ProphetTable::dump_table");
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
    oasys::ScopeLock l(lock_,"ProphetTable::truncate");
    for(iterator i = table_.begin();
        i != table_.end();
        i++)
    {
        ProphetNode* node = (*i).second;
        if (node->p_value() < epsilon)
        {
            table_.erase(i);
            store_del(node);
            delete node;
        }
    }
}

void
ProphetTable::free()
{
    oasys::ScopeLock l(lock_,"ProphetTable::free");
    for(iterator i = table_.begin();
        i != table_.end();
        i++)
    {
        delete((*i).second);
    }
}

int
ProphetTable::age_nodes()
{
    oasys::ScopeLock l(lock_,"ProphetTable::age_nodes");
    int c = 0;
    for(iterator i = table_.begin();
        i != table_.end();
        i++)
    {
        ((*i).second)->update_age();
        c++;
    }
    return c;
}

void
ProphetTableAgeTimer::reschedule()
{
    struct timeval when;
    ::gettimeofday(&when,0);
    when.tv_sec += period_;
    schedule_at(&when);
}

void
ProphetTableAgeTimer::timeout(const struct timeval& now)
{
    (void)now;
    int c = table_->age_nodes();
    table_->truncate(epsilon_);
    reschedule();
    log_debug("aged %d prophet nodes",c);
}

void
ProphetAckAgeTimer::reschedule()
{
    struct timeval when;
    ::gettimeofday(&when,0);
    when.tv_sec += period_;
    schedule_at(&when);
}

void
ProphetAckAgeTimer::timeout(const struct timeval& now)
{
    (void)now;
    log_debug("aged %zu of %zu prophet ACKs",list_->expire(),list_->size());
    reschedule();
}

void
ProphetDictionary::dump(oasys::StringBuffer* buf)
{
    buf->appendf("0 %s\n",sender_.c_str());
    buf->appendf("1 %s\n",receiver_.c_str());
    for(const_iterator i = rribd_.begin(); i != rribd_.end(); i++)
    {
        ASSERT(EndpointID::NULL_EID().equals((*i).second) == false);
        // print out SID -> EndpointID
        buf->appendf("%d %s\n",(*i).first,(*i).second.c_str());
    }
}

void
BundleOfferList::dump(oasys::StringBuffer* buf)
{
    for(const_iterator i = list_.begin(); i != list_.end(); i++)
    {
        (*i)->dump(buf);
    }
}

ProphetDictionary::ProphetDictionary(const EndpointID& sender,
                                     const EndpointID& receiver)
    : sender_(sender), receiver_(receiver), guess_(0)
{
    // neither sender nor receiver gets encoded into RIBDTLV
    // so no call to update_guess
}

ProphetDictionary::ProphetDictionary(const ProphetDictionary& pd)
    : sender_(pd.sender_), receiver_(pd.receiver_), ribd_(pd.ribd_),
      rribd_(pd.rribd_), guess_(pd.guess_)
{
}

bool
ProphetDictionary::is_assigned(const EndpointID& eid) const
{
    ASSERT(eid.equals(EndpointID::NULL_EID()) == false);
    if (sender_ == eid || receiver_ == eid)
    {
        return true;
    }
    ribd::const_iterator it =
        (ribd::const_iterator) ribd_.find(eid);
    if (it != ribd_.end())
    {
        return true;
    }
    return false;
}

u_int16_t
ProphetDictionary::find(const EndpointID& eid) const
{
    ASSERT(eid.equals(EndpointID::NULL_EID()) == false);
    if (sender_ == eid) 
    {
        return 0;
    }
    if (receiver_ == eid)
    {
        return 1;
    }
    ribd::const_iterator it =
        (ribd::const_iterator) ribd_.find(eid);
    if (it != ribd_.end()) {
        return (*it).second;
    }
    return INVALID_SID;
}

EndpointID
ProphetDictionary::find(u_int16_t id) const
{
    if (id == 0)
    {
        return sender_;
    }
    if (id == 1)
    {
         return receiver_;
    }
    if (id == INVALID_SID)
    {
        return EndpointID::NULL_EID();
    }
    rribd::const_iterator it = (rribd::const_iterator) rribd_.find(id);
    if (it != rribd_.end()) {
        return (*it).second;
    }
    return EndpointID::NULL_EID();
}

u_int16_t
ProphetDictionary::insert(const EndpointID& eid)
{
    ASSERT(eid.equals(EndpointID::NULL_EID()) == false);
    // skip past the implicit sender & receiver
    u_int16_t sid = ribd_.size() + 2;
    ASSERTF(sid != INVALID_SID,"dictionary full!");
    bool res = assign(eid,sid);
    return res ? sid : 0;
}

bool
ProphetDictionary::assign(const EndpointID& eid, u_int16_t sid)
{
    ASSERT(eid.equals(EndpointID::NULL_EID()) == false);

    // validate internal state
    ASSERT(ribd_.size() == rribd_.size());

    // enforce definition of RIBD
    if (sid == 0)
    {
        // should not already be defined
        ASSERT(sender_.equals(EndpointID::NULL_EID()));
        // should be a valid eid
        ASSERT(eid.equals(EndpointID::NULL_EID()) == false);
        return sender_.assign(eid);
    }
    if (sid == 1)
    {
        // should not already be defined
        ASSERT(receiver_.equals(EndpointID::NULL_EID()));
        // should be a valid eid
        ASSERT(eid.equals(EndpointID::NULL_EID()) == false);
        return receiver_.assign(eid);
    }

    if (sid == INVALID_SID)
    {
        log_err_p("/dtn/route/prophet",
                  "Attempt to assign %s to INVALID_SID %u",
                  eid.c_str(),sid);
        return false;
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
        update_guess(eid.str().size());
    }
    ASSERTF(ribd_.size() == rribd_.size(),
            "%zu != %zu",ribd_.size(),rribd_.size());
    return res;
}

void
ProphetDictionary::clear() {
    // get rid of the old dictionary
    sender_ = EndpointID::NULL_EID();
    receiver_ = EndpointID::NULL_EID();
    ribd_.clear();
    rribd_.clear();
    guess_ = 0;
}

void
BundleOfferList::sort(const ProphetDictionary* ribd,
                      ProphetTable* nodes,
                      u_int16_t sid)
{
    oasys::ScopeLock l(lock_,"BundleOfferList::sort");
    std::sort(list_.begin(),list_.end(),BundleOfferSIDComp(ribd,nodes,sid));
}

bool
BundleOfferList::remove_bundle(u_int32_t cts, u_int32_t seq, u_int16_t sid)
{
    oasys::ScopeLock l(lock_,"BundleOfferList::remove_bundle");
    for (iterator i = list_.begin(); 
         i != list_.end();
         i++)
    {
        if ((*i)->creation_ts() == cts &&
            (*i)->seqno() == seq &&
            (*i)->sid() == sid)
        {
            list_.erase(i);
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
    if (find(bo->creation_ts(),bo->seqno(),bo->sid())==NULL)
    {
        list_.push_back(bo);
    }
}

void
BundleOfferList::add_offer(BundleOffer* offer)
{
    ASSERT(type_ != BundleOffer::UNDEFINED);
    ASSERT(offer->type() == type_);
    push_back(new BundleOffer(*offer));
}

void
BundleOfferList::add_offer(u_int32_t cts, u_int32_t seq, u_int16_t sid,
        bool custody, bool accept, bool ack)
{
    ASSERT(type_ != BundleOffer::UNDEFINED);
    push_back(new BundleOffer(cts, seq, sid, custody, accept, ack, type_));
}

void
BundleOfferList::add_offer(Bundle* bundle,u_int16_t sid)
{
    BundleRef b("BundleOfferList::add_offer");
    b = bundle;
    if (b.object() == NULL) return;
    ASSERT(type_ != BundleOffer::UNDEFINED);
    push_back(new BundleOffer(b->creation_ts_.seconds_,
                              b->creation_ts_.seqno_, sid,
                              b->custody_requested_, false, false, type_));
}

BundleOffer*
BundleOfferList::find(u_int32_t cts, u_int32_t seq, u_int16_t sid) const
{
    oasys::ScopeLock l(lock_,"BundleOfferList::find");
    BundleOffer* retval = NULL;
    for (const_iterator i = list_.begin(); 
         i != list_.end();
         i++)
    {
        if ((*i)->creation_ts() == cts &&
            (*i)->seqno() == seq &&
            (*i)->sid() == sid)
        {
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
    : lock_(new oasys::SpinLock()) {}

ProphetAckList::~ProphetAckList()
{
    {
        // clean up memory allocated by this list
        oasys::ScopeLock l(lock_,"ProphetAckList::destructor");
        for(palist::iterator iter = acks_.begin();
            iter != acks_.end();
            iter++)
        {
            delete (*iter++);
        }
    }
    acks_.clear();
    delete lock_;
}

size_t
ProphetAckList::count(const EndpointID& eid) const
{
    oasys::ScopeLock l(lock_,"ProphetAckList::count");
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
ProphetAckList::insert(const EndpointID& eid, u_int32_t cts,
                       u_int32_t seq, u_int32_t ets)
{
    // keep ACK for one day unless spec'd otherwise
    if (ets == 0)
        ets = 86400;
    return insert(new ProphetAck(eid,cts,seq,ets));
}

bool
ProphetAckList::insert(Bundle* b)
{
    return insert(Prophet::eid_to_routeid(b->dest_),
                  b->creation_ts_.seconds_,
                  b->creation_ts_.seqno_,
                  b->expiration_);
}

bool
ProphetAckList::insert(ProphetAck* p)
{
    oasys::ScopeLock l(lock_,"ProphetAckList::insert");
    if (acks_.insert(p).second)
    {
        return true;
    }
    delete p;
    return false;
}

size_t
ProphetAckList::expire()
{
    oasys::ScopeLock l(lock_,"ProphetAckList::expire");
    size_t how_many = 0;
    u_int32_t now = BundleTimestamp::get_current_time();
    palist::iterator iter = acks_.begin();
    while(iter != acks_.end()) {
        ProphetAck* p = *iter;
        if ((now - p->cts_) > p->ets_) {
            how_many++;
            acks_.erase(iter++);
            delete p;
        } else
            iter++;
    }
    return how_many;
}

size_t
ProphetAckList::fetch(const EndpointIDPattern& eid,
                      PointerList<ProphetAck>& list) const
{
    oasys::ScopeLock l(lock_,"ProphetAckList::fetch");
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
ProphetAckList::is_ackd(const EndpointID& eid,
                        u_int32_t cts,
                        u_int32_t seq) const
{
    oasys::ScopeLock l(lock_,"ProphetAckList::fetch");
    ProphetAck p(eid);
    palist::iterator iter = acks_.lower_bound(&p);
    while (iter != acks_.end())
    {
        if (!(*iter)->dest_id_.equals(eid))
            break;
        if ((*iter)->cts_ == cts &&
            (*iter)->seq_ == seq) 
            return true;
    }
    return false;
}

bool
ProphetAckList::is_ackd(Bundle* b) const
{
    return is_ackd(
            Prophet::eid_to_routeid(b->dest_),
            b->creation_ts_.seconds_,
            b->creation_ts_.seqno_);
}

ProphetStats::~ProphetStats()
{
    // For each member of pstats, delete the new'd memory
    {
        oasys::ScopeLock l(lock_,"ProphetStats::destructor");
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
    ASSERT(lock_->is_locked_by_me());
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
    oasys::ScopeLock l(lock_,"ProphetStats::update_stats");
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
    oasys::ScopeLock l(lock_,"ProphetStats::get_p_max");
    ProphetStatsEntry* pse = find_entry(b);
    return pse->p_max_;
}

double
ProphetStats::get_mopr(const Bundle* b)
{
    oasys::ScopeLock l(lock_,"ProphetStats::get_mopr");
    ProphetStatsEntry* pse = find_entry(b);
    return pse->mopr_;
}

double
ProphetStats::get_lmopr(const Bundle* b)
{
    oasys::ScopeLock l(lock_,"ProphetStats::get_lmopr");
    ProphetStatsEntry* pse = find_entry(b);
    return pse->lmopr_;
}

void
ProphetStats::drop_bundle(const Bundle* b)
{
    oasys::ScopeLock l(lock_,"ProphetStats::drop_bundle");
    ProphetStatsEntry* pse = NULL;
    iterator it = pstats_.find(b->bundleid_);
    if (it != pstats_.end())
    {
        pse = (*it).second;
        pstats_.erase(it);
        delete pse;
        dropped_++;
    }
}

Bundle*
ProphetBundleList::find(const BundleList& list,
                        const EndpointID& dest,
                        u_int32_t cts,
                        u_int32_t seq)
{
    oasys::ScopeLock l(list.lock(), "ProphetBundleList::find");
    EndpointIDPattern route = Prophet::eid_to_route(dest);
    for(BundleList::const_iterator i =
            (BundleList::const_iterator) list.begin();
        i != list.end();
        i++)
    {
        if ((*i)->creation_ts_.seconds_ == cts &&
            (*i)->creation_ts_.seqno_ == seq &&
            route.match((*i)->dest_))
        {
            return *i;
        }
    }
    return NULL;
}

} // namespace dtn

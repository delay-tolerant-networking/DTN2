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

#ifndef _DTN_PROPHET_LISTS_
#define _DTN_PROPHET_LISTS_

#include "routing/Prophet.h"
#include "routing/ProphetNode.h"
#include <oasys/debug/Log.h>
#include <oasys/util/Time.h>
#include <oasys/thread/SpinLock.h>
#include "naming/EndpointID.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleActions.h"
#include "contacts/Link.h"
#include <oasys/util/BoundedPriorityQueue.h>
#include <oasys/util/URL.h>

#include <vector>
#include <map>
#include <set>

#define FOUR_BYTE_ALIGN(x) (((x) % 4) != 0) ? ((x) + (4 - ((x) % 4))) : (x)

/**
 * Pages and paragraphs refer to IETF Prophet, March 2006
 */

namespace dtn {

/**
 * Common parameters shared between the one ProphetController and the
 * many ProphetEncounters.
 */
struct ProphetParams : public ProphetNodeParams
{
    ProphetParams()
        : ProphetNodeParams(),
          fs_(Prophet::GRTR),
          qp_(Prophet::FIFO),
          hello_interval_(Prophet::HELLO_INTERVAL),
          hello_dead_(Prophet::HELLO_DEAD),
          max_forward_(Prophet::DEFAULT_NUM_F_MAX),
          min_forward_(Prophet::DEFAULT_NUM_F_MIN),
          max_usage_(0xffffffff),
          age_period_(Prophet::AGE_PERIOD),
          relay_node_(false),
          custody_node_(false),
          internet_gw_(false),
          epsilon_(0.0039) {}
          
    Prophet::fwd_strategy_t fs_;      ///< bundle forwarding strategy
    Prophet::q_policy_t     qp_;      ///< bundle queuing policy

    u_int8_t hello_interval_; ///< delay between HELLO beacons (100ms units)
    u_int   hello_dead_;     ///< hello_intervals before peer considered offline

    u_int   max_forward_;    ///< max times to forward bundle using GTMX
    u_int   min_forward_;    ///< min times to forward before dropping (LEPR)
    u_int   max_usage_;      ///< max bytes to allow consumed by bundles

    u_int   age_period_;     ///< seconds between aging cycles (Section 3.8)

    bool   relay_node_;     ///< whether Prophet bridges to non-Prophet domains
    bool   custody_node_;   ///< whether this node will accept custody txfrs
    bool   internet_gw_;    ///< whether this node bridges to Internet domain
    double epsilon_;        ///< minimum allowed pvalue before dropping node
};

/**
 * Auto deletes pointers in list destructor
 * This object assumes ownership for member pointers
 * Creates copies of members instead of copies of pointers to members
 */
template <class T>
class PointerList : public std::vector<T*>
{
public:
    typedef std::vector<T*> List;
    typedef typename std::vector<T*>::iterator iterator;
    typedef typename std::vector<T*>::const_iterator const_iterator;

    /**
     * Default constructor
     */
    PointerList()
        : std::vector<T*>() {}

    /**
     * Copy constructor
     */
    PointerList(const PointerList& a)
        : std::vector<T*>()
    {
        clear();
        copy_from(a);
    }

    /**
     * Destructor
     */
    virtual ~PointerList()
    {
        clear();
    }

    /**
     * Assignment operator creates deep copy, not pointer copy
     */
    PointerList& operator= (const PointerList& a)
    {
        clear();
        copy_from(a);
        return *this;
    }

    /**
     * Deletes member pointed to by iterator, then removes pointer
     */
    void erase(iterator i)
    {
        delete (*i);
        List::erase(i);
    }

    /**
     * Delete all member variables, then remove pointers from 
     * container
     */
    void clear()
    {
        free();
        List::clear();
    }
protected:

    /**
     * Free memory pointed to by member variables
     */
    void free()
    {
        for(iterator i = List::begin();
            i != List::end();
            i++)
        {
            delete *i;
        }
    }

    /**
     * Utility function to perform deep copy from peer object
     */
    void copy_from(const PointerList& a)
    {
        for(const_iterator i = a.begin();
            i != a.end();
            i++)
        {
            push_back(new T(**i));
        }
    }
}; // template PointerList

typedef PointerList<ProphetNode> ProphetNodeList;

/**
 * Comparator for EndpointID sequencing
 */
struct less_eid_ 
{
    bool operator() (const EndpointID& a, const EndpointID& b) const
    {
        return a.str() < b.str();
    }
};

/**
 * Container for pvalues. Assumes ownership of memory pointed to by Node*
 */
class ProphetTable
{
public:
    typedef std::map<EndpointID,ProphetNode*,less_eid_> rib_table;
    typedef std::map<EndpointID,ProphetNode*,less_eid_>::iterator iterator;

    /**
     * Default constructor
     */
    ProphetTable();

    /**
     * Destructor
     */
    ~ProphetTable();

    /**
     * Given an EndpointID, returns pointer to ProphetNode if found,
     * else returns NULL
     */
    ProphetNode* find(const EndpointID& eid) const;

    /**
     * Given a pointer to ProphetNode*, assumes ownership of memory pointed
     * to by *node, updates member (replacing if exists)
     */
    void update(ProphetNode* node);

    /**
     * Convenience functions for looking up predictability value
     */
    ///@{
    double p_value(const Bundle* b) const
    {
        return p_value(Prophet::eid_to_routeid(b->dest_));
    }
    double p_value(const EndpointID& eid) const;
    ///@}

    /**
     * Create copy of list 
     */
    size_t dump_table(ProphetNodeList& list) const;

    /**
     * Returns number of members in ProphetTable
     */
    size_t size() const { return table_.size(); }

    /**
     * Frees memory pointed to by member nodes, then removes node pointers
     * from list
     */
    void clear() { free(); table_.clear(); }

    /**
     * Caller must hold lock() when calling iterator
     */
    iterator begin();

    /**
     * Caller must hold lock() when calling iterator
     */
    iterator end();

    /**
     * For maintenance routines, remove any nodes with p_value < epsilon
     */
    void truncate(double epsilon);

    /**
     * For maintenance routines, invoke age calculation on each node; return
     * number of nodes visited
     */
    int age_nodes();

    /**
     * Returns a pointer to member SpinLock for synchronizing concurrent
     * access
     */
    oasys::SpinLock* lock() { return lock_; }
protected:

    /**
     * Clean up memory pointed to by member variables
     */
    void free();

    oasys::SpinLock* lock_;
    rib_table table_;
}; // ProphetTable

/**
 * Scans the list of ProphetNodes and applies aging algorithm to the pvalues
 */
class ProphetTableAgeTimer : public oasys::Timer,
                             public oasys::Logger
{
public:
    ProphetTableAgeTimer(ProphetTable* table, u_int period, double epsilon) :
        oasys::Logger("ProphetTableAgeTimer","/dtn/router/prophet/timer"),
        table_(table), period_(period), epsilon_(epsilon)
    { reschedule(); }
    void timeout(const struct timeval& now);
protected:
    void reschedule();
    ProphetTable* table_;
    u_int period_;
    double epsilon_;
}; // ProphetTableAgeTimer 

/**
 * Utility class to facilitate translation to and from EndpointIDs and
 * their 16-bit string IDs
 */
class ProphetDictionary
{
public:
    typedef std::map<u_int16_t,EndpointID> rribd;
    typedef rribd::const_iterator const_iterator;

    static const u_int16_t INVALID_SID;

    ProphetDictionary(const EndpointID& sender = EndpointID::NULL_EID(),
                      const EndpointID& receiver = EndpointID::NULL_EID());
    ProphetDictionary(const ProphetDictionary& pd);
    ~ProphetDictionary() {;}

    /**
     * If eid is valid, returns sid; else zero
     */
    u_int16_t find(const EndpointID& eid) const;

    /**
     * If id is valid, returns EID; else NULL_EID
     */
    EndpointID find(u_int16_t id) const;

    /**
     * Convenience function
     */
    const EndpointID& sender() const { return sender_; }

    /**
     * Convenience function
     */
    const EndpointID receiver() const { return receiver_; }

    /**
     * If EID is already indexed, return true; else false
     */
    bool is_assigned(const EndpointID& eid) const;

    /**
     * Attempts to insert EID, returns SID upon success (zero 
     * if collision)
     */
    u_int16_t insert(const EndpointID& eid);

    /**
     * Assign EID to arbitrary SID; returns true upon success
     * (false if collision)
     */
    bool assign(const EndpointID& eid, u_int16_t sid);

    /**
     * Iterator (std::pair<u_int16_t,EndpointID>)
     */
    const_iterator begin() const { return rribd_.begin(); }
    
    /**
     * Iterator (std::pair<u_int16_t,EndpointID>)
     */
    const_iterator end() const { return rribd_.end(); }
    
    /**
     * Return the number of elements in the dictionary
     */
    size_t size() const { return ribd_.size(); }

    /**
     * Helper function for serializing dictionary
     */
    size_t guess_ribd_size() const { return guess_; }

    /**
     * Wipe out dictionary (prepare to deserialize new one)
     */
    void clear();

    /**
     * Write out text representation to string buffer
     */
    void dump(oasys::StringBuffer *buf);

    ProphetDictionary& operator= (const ProphetDictionary& d)
    {
        sender_ = d.sender_;
        receiver_ = d.receiver_;
        ribd_ = d.ribd_;
        rribd_ = d.rribd_;
        guess_ = d.guess_;
        return *this;
    }
protected:
    typedef std::map<EndpointID,u_int16_t,less_eid_> ribd;

    void update_guess(size_t len) {
        guess_ += FOUR_BYTE_ALIGN(len + Prophet::RoutingAddressStringSize);
    }

    EndpointID sender_;
    EndpointID receiver_;
    ribd      ribd_;
    rribd     rribd_;
    size_t    guess_;
}; // ProphetDictionary

struct BundleOfferComp : public std::less<BundleOffer*>
{
    BundleOfferComp(const ProphetDictionary* ribd, ProphetTable* local)
        : ribd_(ribd), nodes_(local) {}

    bool operator()(const BundleOffer* a, const BundleOffer* b) const
    {
        const EndpointID ea = ribd_->find(a->sid()); 
        const EndpointID eb = ribd_->find(b->sid());
        return nodes_->p_value(ea) > nodes_->p_value(eb);
    }
    
    const ProphetDictionary* ribd_;
    ProphetTable* nodes_;
};

struct BundleOfferSIDComp : public BundleOfferComp
{
    BundleOfferSIDComp(const ProphetDictionary* ribd,
                       ProphetTable* local,
                       u_int16_t sid)
        : BundleOfferComp(ribd,local), sid_(sid) {}

    bool operator()(const BundleOffer* a,const BundleOffer* b) const
    {
        // prioritize based on SID
        if (a->sid() != b->sid())
        {
            if (a->sid() == sid_) return true;
            if (b->sid() == sid_) return true;
        }
        // otherwise use parent's ordering
        return BundleOfferComp::operator()(a,b);
    }
    u_int16_t sid_;
};

/**
 * BundleOfferList represents a BundleOfferTLV as received from 
 * or sent to remote
 */
class BundleOfferList {
public:
    typedef PointerList<BundleOffer> List;
    typedef PointerList<BundleOffer>::iterator iterator;
    typedef PointerList<BundleOffer>::const_iterator const_iterator;

    BundleOfferList(BundleOffer::bundle_offer_t type =
                    BundleOffer::UNDEFINED)
        : type_(type), lock_(new oasys::SpinLock())
    {
    }

    BundleOfferList(const BundleOfferList& list)
        : list_(list.list_),
          type_(list.type_),
          lock_(new oasys::SpinLock())
    {
    }

    ~BundleOfferList()
    {
        list_.clear();
        delete lock_;
    }

    /**
     * Use delivery predictability to sort requests in priority order
     * then move local-destined requests to front of resultant list
     */
    void sort(const ProphetDictionary* ribd, ProphetTable* nodes, u_int16_t sid);

    /**
     * Returns number of entries in list
     */
    size_t size() const;

    /**
     * Returns true if list is empty
     */
    bool empty() const;

    /**
     * Removes all entries from list
     */
    void clear();

    /**
     * Add an entry from BundleOfferList
     */
    void add_offer(BundleOffer* entry);

    /**
     * Remove an entry from BundleOfferList; true if found (and removed)
     * else false if did not exist
     */
    bool remove_bundle(u_int32_t cts, u_int32_t seq, u_int16_t sid);

    /**
     * Convenience function to add an entry to BundleOfferList
     */
    void add_offer(u_int32_t cts, u_int32_t seq, u_int16_t sid,
                   bool custody=false, bool accept=false, bool ack=false);

    /**
     * Convenience function to add an entry to BundleOfferList
     */
    void add_offer(Bundle* b,u_int16_t sid);

    /**
     * Return pointer to BundleOffer if found, else NULL
     */
    BundleOffer* find(u_int32_t cts, u_int32_t seq, u_int16_t sid) const;

    BundleOffer::bundle_offer_t type() { return type_; }

    /**
     * Iterators require caller to hold lock
     */
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    /**
     * Get first pointer from list, else return NULL
     */
    BundleOffer* front() const
    {
        oasys::ScopeLock l(lock_,"front");
        return list_.front();
    }

    /**
     * Estimate serialized buffer length
     */ 
    size_t guess_size()
    {
        return Prophet::BundleOfferEntrySize * list_.size();
    }

    BundleOfferList& operator= (const BundleOfferList& a)
    {
        oasys::ScopeLock al(a.lock_,"operator=");
        oasys::ScopeLock l(lock_,"operator=");
        list_ = a.list_;
        type_ = a.type_;
        return *this;
    }

    oasys::SpinLock* lock() { return lock_; }

    void set_type(BundleOffer::bundle_offer_t type) {type_ = type;}
    BundleOffer::bundle_offer_t type() const { return type_; }

    void dump(oasys::StringBuffer *buf);

protected:
    /**
     * Adds entry to back of list
     */
    void push_back(BundleOffer* bo);

    List list_;
    BundleOffer::bundle_offer_t type_;
    oasys::SpinLock* lock_;
}; // BundleOfferList

/**
 * Section 3.5 (p. 16) describes Prophet ACKs as needing to persist in a node's
 * storage beyond the lifetime of the bundle they represent.  ProphetAckList
 * is that persistence (but not [yet] serializable to permanent storage).
 */
class ProphetAckList {
public:
    struct less_ack_ {
        bool operator() (const ProphetAck* a, const ProphetAck* b) const {
            return *a < *b;
        }
    };
    typedef std::set<ProphetAck*,less_ack_> palist;

    ProphetAckList();
    ProphetAckList(const ProphetAckList& a) 
        : acks_(a.acks_) {;}
    ~ProphetAckList();

    /**
     * Insert a ProphetAck with the provided attributes; return true if
     * inserted, else false if ACK already exists
     *
     * Expiration is a time difference in sec, as with dtn::Bundle; if ets
     * is 0, then one day is used (86,400 sec)
     */
    bool insert(const EndpointID& eid,
                u_int32_t cts,
                u_int32_t seq = 0,
                u_int32_t ets = 0);

    /**
     * Convenience wrapper
     */
    bool insert(Bundle *b);

    /**
     * Insert ProphetAck; return true on success, else false if ACK 
     * already exists
     */
    bool insert(ProphetAck* ack);

    /**
     * Return the number of ACKs known for a given EID
     */
    size_t count(const EndpointID& eid) const;

    /**
     * Place the set of ACKs for given EID in list, return number found
     */
    size_t fetch(const EndpointIDPattern& eid,
                 PointerList<ProphetAck>& list) const;

    /**
     * Answer whether this Bundle has been ACK'd
     */
    bool is_ackd(const EndpointID& eid, u_int32_t cts, u_int32_t seq = 0) const;
    bool is_ackd(Bundle* b) const;

    /**
     * Visit every ACK in the list, and delete those for which the
     * expiration has passed; return the number of elements deleted
     */
    size_t expire();

    /**
     * Number of elements contained
     */
    size_t size() const { return acks_.size(); }
protected:
    palist acks_;
    oasys::SpinLock* lock_;
}; // ProphetAckList

/**
 * Action to expire out aged ProphetAcks from ProphetAckList
 */
class ProphetAckAgeTimer :
    public oasys::Timer,
    public oasys::Logger
{
public:
    ProphetAckAgeTimer(ProphetAckList* list, u_int period) :
        oasys::Logger("ProphetAckAgeTimer","/dtn/router/prophet/timer"),
        list_(list), period_(period)
    { reschedule(); }
    void timeout(const struct timeval& now);
protected:
    void reschedule();
    ProphetAckList* list_;
    u_int period_;
}; // ProphetAckAgeTimer

struct ProphetStatsEntry {
    double p_max_;
    double mopr_;
    double lmopr_;
};

class ProphetStats {
public:
    ProphetStats() : dropped_(0), lock_(new oasys::SpinLock())
    {
        pstats_.clear();
    }

    ~ProphetStats();

    void update_stats(const Bundle* b, double p);
    double get_p_max(const Bundle* b);
    double get_mopr(const Bundle* b);
    double get_lmopr(const Bundle* b);

    void drop_bundle(const Bundle* b);
    u_int dropped() { return dropped_; }

protected:
    typedef std::map<u_int32_t,ProphetStatsEntry*> pstats;
    typedef std::map<u_int32_t,ProphetStatsEntry*>::iterator iterator;
    typedef std::map<u_int32_t,ProphetStatsEntry*>::const_iterator
        const_iterator;

    ProphetStatsEntry* find_entry(const Bundle*);

    u_int   dropped_;
    pstats pstats_;
    oasys::SpinLock* lock_;
}; // ProphetStats

/**
 * Forwarding strategy null comparator (GRTR, GTMX, GRTR_PLUS, GTMX_PLUS)
 */
class FwdStrategy : public std::less<Bundle*>
{
public:
    virtual ~FwdStrategy() {}
    FwdStrategy(const FwdStrategy& fs)
        : fs_(fs.fs_)
    {}
    bool operator() (const Bundle*, const Bundle*) const
    {
        return false;
    }
    inline static FwdStrategy* strategy( Prophet::fwd_strategy_t fs,
                                         ProphetTable* local = NULL,
                                         ProphetTable* remote = NULL );
protected:
    FwdStrategy(Prophet::fwd_strategy_t fs = Prophet::INVALID_FS)
        : fs_(fs)
    {}
    Prophet::fwd_strategy_t fs_;
};

/**
 * Forwarding strategy comparator for GRTR_SORT
 */
class FwdStrategyCompGRTRSORT : public FwdStrategy
{
public:
    FwdStrategyCompGRTRSORT (const FwdStrategyCompGRTRSORT& fsc)
        : FwdStrategy(fsc), local_(fsc.local_), remote_(fsc.remote_)
    {}
    bool operator() (const Bundle* a, const Bundle* b) const
    {
        return ((remote_->p_value(a) - local_->p_value(a)) <
                (remote_->p_value(b) - local_->p_value(b)));
    }

protected:
    friend class FwdStrategy;
    FwdStrategyCompGRTRSORT(ProphetTable* local,ProphetTable* remote)
        : local_(local), remote_(remote)
    {
        ASSERT(local != NULL);
        ASSERT(remote != NULL);
    }

    ProphetTable* local_;
    ProphetTable* remote_;
};

/**
 * Forwarding strategy comparator for GRTR_MAX
 */
class FwdStrategyCompGRTRMAX : public FwdStrategy
{
public:
    FwdStrategyCompGRTRMAX(const FwdStrategyCompGRTRMAX& f)
        : FwdStrategy(f), remote_(f.remote_) {}
    bool operator() (const Bundle* a, const Bundle* b) const
    {
        return (remote_->p_value(a) < remote_->p_value(b));
    }

protected:
    friend class FwdStrategy;
    FwdStrategyCompGRTRMAX(ProphetTable* remote)
        : remote_(remote)
    {
        ASSERT(remote != NULL);
    }

    ProphetTable* remote_;
};

/**
 * Factory method for strategy comparators
 */
FwdStrategy*
FwdStrategy::strategy( Prophet::fwd_strategy_t fs,
                       ProphetTable* local,
                       ProphetTable* remote )
{
    FwdStrategy *f = NULL;
    switch (fs) 
    {
        case Prophet::GRTR:
        case Prophet::GTMX:
        case Prophet::GRTR_PLUS:
        case Prophet::GTMX_PLUS:
            f = new FwdStrategy();
            break;
        case Prophet::GRTR_SORT:
            //f = (FwdStrategy*) new FwdStrategyCompGRTRSORT(local,remote);
            f = new FwdStrategyCompGRTRSORT(local,remote);
            break;
        case Prophet::GRTR_MAX:
            //f = (FwdStrategy*) new FwdStrategyCompGRTRMAX(remote);
            f = new FwdStrategyCompGRTRMAX(remote);
            break;
        default:
            PANIC("Invalid forwarding strategy: %d",(int)fs);
            break;
    }
    return f;
};

/**
 * Base class for Prophet forwarding strategy decision makers
 */
class ProphetDecider : public oasys::Logger
{
public:
    inline static ProphetDecider* decider(
        Prophet::fwd_strategy_t fs,
        ProphetTable* local = NULL,
        ProphetTable* remote = NULL,
        const LinkRef& nexthop = ProphetDecider::null_link_,
        u_int max_forward = 0,
        ProphetStats* stats = NULL);
    virtual ~ProphetDecider() {}
    virtual bool operator() (const Bundle*) const = 0;
    inline bool should_fwd(const Bundle* bundle) const;
protected:
    ProphetDecider(const LinkRef& nexthop)
        : oasys::Logger("ProphetDecider","/dtn/route/decider"),
          next_hop_(nexthop.object(), "ProphetDecider"),
          route_(Prophet::eid_to_route(nexthop->remote_eid()))
    {}

    static const LinkRef null_link_;
    
    LinkRef next_hop_;
    EndpointIDPattern route_;
};

/**
 * Decision maker class on whether to forward a bundle
 */

class FwdDeciderGRTR : public ProphetDecider
{
public:
    bool operator() (const Bundle* b) const
    {
        if (!ProphetDecider::should_fwd(b))
            return false;
        if (route_.match(b->dest_))
            return true;
        double local_p = local_->p_value(b);
        double remote_p = remote_->p_value(b);
        if (local_p < remote_p)
        {
            log_debug("remote p %0.2f local p %0.2f: ok to fwd *%p",
                      remote_p,local_p,b);
            return true;
        }
        log_debug("remote p %0.2f local p %0.2f: do not fwd *%p",
                  remote_p,local_p,b);
        return false;
    }

    virtual ~FwdDeciderGRTR() {}
protected:
    friend class ProphetDecider;
    FwdDeciderGRTR(ProphetTable* local, ProphetTable* remote,
                   const LinkRef& nexthop)
        : ProphetDecider(nexthop), local_(local), remote_(remote)
    {
        ASSERT(local != NULL);
        ASSERT(remote != NULL);
    }

    ProphetTable* local_;
    ProphetTable* remote_;
};

class FwdDeciderGTMX : public FwdDeciderGRTR
{
public:
    bool operator() (const Bundle* b) const
    {
        if ( ! FwdDeciderGRTR::operator()(b) )
            return false;
        if (route_.match(b->dest_))
            return true;
        size_t num_fwd = 
            b->fwdlog_.get_transmission_count(ForwardingInfo::COPY_ACTION);
        if (num_fwd < max_fwd_)
        {
            log_debug("NF %zu, max NF %d: ok to fwd *%p",num_fwd,max_fwd_,b);
            return true;
        }
        log_debug("NF %zu, max NF %d: do not fwd *%p",num_fwd,max_fwd_,b);
        return false;
    }
    
    virtual ~FwdDeciderGTMX() {}
protected:
    friend class ProphetDecider;
    FwdDeciderGTMX(ProphetTable* local, ProphetTable* remote,
                   const LinkRef& nexthop, u_int max_forward)
        : FwdDeciderGRTR(local,remote,nexthop),
          max_fwd_(max_forward)
    {}

    u_int max_fwd_;
};

class FwdDeciderGRTRPLUS : public FwdDeciderGRTR
{
public:
    bool operator() (const Bundle* b) const
    {
        if ( ! FwdDeciderGRTR::operator()(b) )
            return false;
        if (route_.match(b->dest_))
            return true;
        bool ok = stats_->get_p_max(b) < remote_->p_value(b);
        log_debug("max P %0.2f, remote P %0.2f, %s fwd *%p",
                  stats_->get_p_max(b),remote_->p_value(b),
                  ok ? "ok to" : "do not", b);
        return ok;
    }

    virtual ~FwdDeciderGRTRPLUS() {}
protected:
    friend class ProphetDecider;
    FwdDeciderGRTRPLUS(ProphetTable* local, ProphetTable* remote,
                       const LinkRef& nexthop, ProphetStats* stats)
        : FwdDeciderGRTR(local,remote,nexthop),
          stats_(stats)
    {
        ASSERT(stats != NULL);
    }

    ProphetStats* stats_;
};

class FwdDeciderGTMXPLUS : public FwdDeciderGRTRPLUS
{
public:
    bool operator() (const Bundle* b) const
    {
        if ( ! FwdDeciderGRTRPLUS::operator()(b) )
            return false;
        if (route_.match(b->dest_))
            return true;
        size_t num_fwd = 
            b->fwdlog_.get_transmission_count(ForwardingInfo::COPY_ACTION);
        bool ok = ((stats_->get_p_max(b) < remote_->p_value(b)) &&
                   (num_fwd < max_fwd_));
        log_debug("NF %zu, Max NF %d, max P %0.2f, remote P %0.2f, %s fwd *%p",
                  num_fwd,max_fwd_,stats_->get_p_max(b),remote_->p_value(b),
                  ok ? "ok to" : "do not", b);
        return ok;
    }

    virtual ~FwdDeciderGTMXPLUS() {}
protected:
    friend class ProphetDecider;
    FwdDeciderGTMXPLUS(ProphetTable* local, ProphetTable* remote,
                       const LinkRef& nexthop, ProphetStats* stats,
                       u_int max_forward)
        : FwdDeciderGRTRPLUS(local,remote,nexthop,stats), max_fwd_(max_forward)
    {}

    u_int max_fwd_;
};

/**
 * Factory method for creating decider instance
 */
ProphetDecider*
ProphetDecider::decider( Prophet::fwd_strategy_t fs, ProphetTable* local,
                         ProphetTable* remote, const LinkRef& nexthop,
                         u_int max_forward, ProphetStats* stats )
{
    ProphetDecider* pd = NULL;
    switch(fs) {
        case Prophet::GRTR:
        case Prophet::GRTR_SORT:
        case Prophet::GRTR_MAX:
            pd = (ProphetDecider*) new FwdDeciderGRTR(local,remote,nexthop);
            break;
        case Prophet::GTMX:
            pd = (ProphetDecider*) new
                FwdDeciderGTMX(local,remote,nexthop,max_forward);
            break;
        case Prophet::GRTR_PLUS:
            pd = (ProphetDecider*) new
                FwdDeciderGRTRPLUS(local,remote,nexthop,stats);
            break;
        case Prophet::GTMX_PLUS:
            pd = (ProphetDecider*) new
                FwdDeciderGTMXPLUS(local,remote,nexthop,stats,max_forward);
            break;
        default:
            PANIC("Invalid forwarding strategy: %d",(int)fs);
            break;
    }
    return pd;
}

// borrowed (modified) from TableBasedRouter::should_fwd
bool
ProphetDecider::should_fwd(const Bundle* bundle) const
{
    ForwardingInfo info;
    bool found = bundle->fwdlog_.get_latest_entry(next_hop_, &info);

    if (found)
    {
        ASSERT(info.state_ != ForwardingInfo::NONE);
    }
    else
    {
        ASSERT(info.state_ == ForwardingInfo::NONE);
    }

    if (info.state_ == ForwardingInfo::TRANSMITTED ||
        info.state_ == ForwardingInfo::IN_FLIGHT)
    {
        log_debug("should_fwd bundle %d: "
                  "skip %s due to forwarding log entry %s",
                  bundle->bundleid_, next_hop_->name(),
                  ForwardingInfo::state_to_str(
                      (ForwardingInfo::state_t)info.state_));
        return false;
    }

    if (info.state_ == ForwardingInfo::TRANSMIT_FAILED) {
        log_debug("should_fwd bundle %d: "
                  "match %s: forwarding log entry %s TRANSMIT_FAILED %d",
                  bundle->bundleid_, next_hop_->name(),
                  ForwardingInfo::state_to_str((ForwardingInfo::state_t)
                      info.state_),
                  bundle->bundleid_);

    } else {
        log_debug("should_fwd bundle %d: "
                  "match %s: forwarding log entry %s",
                  bundle->bundleid_, next_hop_->name(),
                  ForwardingInfo::state_to_str((ForwardingInfo::state_t)
                      info.state_));
    }

    return true;
}

/**
 * Helper class to enforce forwarding strategies, used to organize
 * BundleOfferTLV 
 */
class ProphetBundleOffer : public oasys::Logger
{
public:
    ProphetBundleOffer(const BundleList& bundles,
                       FwdStrategy* comp,
                       ProphetDecider* decider)
        : oasys::Logger("ProphetBundleOffer","/dtn/route/offer"),
          queue_(*comp),
          list_("ProphetBundleOffer"),comp_(comp),decide_(decider)
    {
        ASSERT(decider != NULL);
        oasys::ScopeLock l(bundles.lock(),"ProphetBundleOffer constructor");
        for(BundleList::const_iterator i =
                (BundleList::const_iterator) bundles.begin();
            i != bundles.end(); 
            i++ )
        {
            push(*i);
        }
    } 
    virtual ~ProphetBundleOffer() {}
    void push(Bundle* b)
    {
        // not every bundle gets forwarded to every neighbor
        if(decide_->operator()(b))
        {
            queue_.push(b);
            list_.push_back(b);
            log_debug("offering *%p (cts:seq %d:%d)",b,
                      b->creation_ts_.seconds_,b->creation_ts_.seqno_);
        }
        else
        {
            log_debug("not offering *%p (cts:seq %d:%d)",b,
                      b->creation_ts_.seconds_,b->creation_ts_.seqno_);
        }
        ASSERT(list_.size() == queue_.size());
    }
    void pop()
    {
        list_.erase(queue_.top());
        queue_.pop();
    }
    bool empty() const
    {
        return queue_.empty();
    }
    Bundle* top() const
    {
        return queue_.top();
    }
    size_t size() const
    { 
        return queue_.size();
    }

protected:
    std::priority_queue<Bundle*> queue_;
    BundleList list_;
    FwdStrategy* comp_;
    ProphetDecider* decide_;
};

/**
 * Accessor pattern for pulling out Bundle size
 */
struct BundleSz {
    u_int operator() (const Bundle* b) const
    {
        return b->payload_.length();
    }
};

struct QueueComp: public std::less<Bundle*> 
{
    inline
    static QueueComp* queuecomp(Prophet::q_policy_t qp,
                                ProphetStats* ps,
                                ProphetTable* pt);
};

/**
 * Queueing policy comparator for FIFO
 */
struct QueueCompFIFO : public QueueComp
{
    bool operator() (const Bundle* a, const Bundle* b) const
    {
        return (a->bundleid_ < b->bundleid_);
    }
};

/**
 * Queueing policy comparator for MOFO
 */
struct QueueCompMOFO : public QueueComp
{
#define NUM_FWD(b) \
        (b)->fwdlog_.get_transmission_count(ForwardingInfo::COPY_ACTION)
    bool operator() (const Bundle* a, const Bundle* b) const
    {
        return (NUM_FWD(a) < NUM_FWD(b));
    }
};

/**
 * Queueing policy comparator for MOPR
 */
class QueueCompMOPR : public QueueComp
{
public:
    QueueCompMOPR(ProphetStats* pstats)
        : pstats_(pstats)
    {
        ASSERT(pstats_ != NULL);
    }
#define MOPR(b) pstats_->get_mopr(b)
    bool operator() (const Bundle* a, const Bundle* b) const
    {
        return MOPR(a) < MOPR(b);
    }
protected:
    ProphetStats* pstats_;
};

/**
 * Queueing policy comparator for LINEAR_MOPR
 */
class QueueCompLMOPR : public QueueComp
{
public:
    QueueCompLMOPR(ProphetStats* pstats)
        : pstats_(pstats)
    {
        ASSERT(pstats_ != NULL);
    }
#define LMOPR(b) pstats_->get_lmopr(b)
    bool operator() (const Bundle* a, const Bundle* b) const
    {
        return (LMOPR(a) < LMOPR(b));
    }
protected:
    ProphetStats* pstats_;
};

/**
 * Queueing policy comparator for SHLI
 */
struct QueueCompSHLI : public QueueComp
{
#define SHLI(b) (b)->expiration_ 
    bool operator() (const Bundle* a, const Bundle* b) const
    {
        return (SHLI(a) < SHLI(b));
    }
};

struct QueueCompLEPR : public QueueComp
{
    QueueCompLEPR(ProphetTable* nodes)
        : nodes_(nodes)
    {
        ASSERT(nodes!=NULL);
    }
#define LEPR(b) nodes_->p_value(b)
    bool operator() (const Bundle* a, const Bundle* b) const
    {
        return (LEPR(a) < LEPR(b));
    }
protected:
    ProphetTable* nodes_;
};

QueueComp*
QueueComp::queuecomp(Prophet::q_policy_t qp,
                     ProphetStats* ps,
                     ProphetTable* pt)
{
    (void) ps;
    (void) pt;
    switch(qp) {
        case Prophet::FIFO:
            return new QueueCompFIFO();
        case Prophet::MOFO:
            return new QueueCompMOFO();
        case Prophet::MOPR:
            return new QueueCompMOPR(ps);
        case Prophet::LINEAR_MOPR:
            return new QueueCompLMOPR(ps);
        case Prophet::SHLI:
            return new QueueCompSHLI();
        case Prophet::LEPR:
            return new QueueCompLEPR(pt);
        case Prophet::INVALID_QP:
        default:
            PANIC("Unrecognized queue policy %d",qp);
    }
}

struct ProphetBundleList
{
    static Bundle* find(const BundleList& list,
                        const EndpointID& dest,
                        u_int32_t creation_ts,
                        u_int32_t seqno);
};

/**
 * Helper class to enforce bundle queueing policies
 * This frankenstein (with BundleList member AND priority_queue's Bundle* 
 * vector) is a temporary hack to increment Bundle references (BundleList)
 * while enforcing Prophet's bundle queueing policy (priority_queue with
 * bound). The "right" answer is to either rewrite BundleList as a template,
 * to allow a vector with which to seed BoundedPriorityQueue ... or largely
 * duplicate BundleList below, in a derived class of priority_queue.
 */
class ProphetBundleQueue :
    public oasys::BoundedPriorityQueue<Bundle*,BundleSz,QueueComp> 
{
public:
    typedef oasys::BoundedPriorityQueue<Bundle*,BundleSz,QueueComp> BundleBPQ;

    ProphetBundleQueue(const BundleList* list,
                       ProphetParams* params,
                       QueueComp comp = QueueCompFIFO())
        : BundleBPQ(comp,params->max_usage_),
          bundles_("ProphetBundleQueue"),
          params_(params)
    {
        ASSERT(list!=NULL);
        ASSERT(params!=NULL);
        
        oasys::ScopeLock l(list->lock(),"ProphetBundleQueue constructor");
        for (BundleList::const_iterator i = list->begin();
             i != list->end();
             i++)
        {
            push(*i);
        }
    }

    virtual ~ProphetBundleQueue() {}

    oasys::SpinLock* lock() const { return bundles_.lock(); }

#define seq_ BundleBPQ::PriorityQueue::c
#define comp_ BundleBPQ::PriorityQueue::comp
    /**
     * copy out a sequence from internal heap; among other purposes, one
     * use for this is to seed BundleOfferTLV's (via ProphetBundleOffer)
     *
     * To export this list, caller must hold lock() on this object
     */
    size_t bundle_list(BundleList& list) const
    {
        ASSERT(lock()->is_locked_by_me());
        // copy out a new sequence of internal heap
        for(BundleList::const_iterator i =
                (BundleList::const_iterator)bundles_.begin();
            i!=bundles_.end();
            i++)
        {
            list.push_back(*i);
        }
        return list.size();
    }

    void drop_bundle(Bundle* b)
    {
        BundleBPQ::Sequence::iterator i = std::find(seq_.begin(),seq_.end(),b);
        if (i != c.end())
        {
            erase_member(i);
            make_heap(seq_.begin(),seq_.end(),comp_);
        }
    }

    void pop()
    {
        // no op
    }

    void push(Bundle* b)
    {
        // prevent duplicates in seq_
        if (bundles_.contains(b))
            return;

        BundleBPQ::push(b);
        bundles_.push_back(b);
    }

    void set_comp(QueueComp* c)
    {
        oasys::ScopeLock l(lock(),"set_comp");
        comp_ = *c;
        make_heap(seq_.begin(),seq_.end(),comp_);
    }

protected:
    void erase_member(iterator pos) {
        BundleBPQ::erase_member(pos);
        bundles_.erase(*pos);
    }

    void enforce_bound() {
        oasys::ScopeLock l(lock(),"enforce_bound");
        // business as usual, unless LEPR
        if (params_->qp_ != Prophet::LEPR) {
            BundleBPQ::enforce_bound();
            return;
        }

        // Apply LEPR queueing policy: things get ugly fast
        // due to the requirement of min_forward_

        // sort
        std::sort_heap(seq_.begin(),seq_.end(),comp_);

        // reverse
        std::reverse(seq_.begin(),seq_.end());

        // find suitable victims until threshold is reached
        for (BundleBPQ::Sequence::iterator i = seq_.begin();
             (i != seq_.end()) && (cur_size_ > max_size_);
             i++)
        {
            if (NUM_FWD(*i) < params_->min_forward_)
                continue;
            cur_size_ -= BundleSz()(*i);
            erase_member(i);
        }
        // reheap the mangled mess
        make_heap(seq_.begin(),seq_.end(),comp_);
    }

    BundleList bundles_;
    ProphetParams* params_;
#undef seq_
#undef comp_
};

}; // dtn

#endif // _DTN_PROPHET_LISTS_

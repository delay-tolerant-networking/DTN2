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

#ifndef _DTN_PROPHET_CONTROLLER_
#define _DTN_PROPHET_CONTROLLER_

#include <oasys/debug/Log.h>
#include <oasys/util/Time.h>
#include <oasys/util/BoundedPriorityQueue.h>
#include "naming/EndpointID.h"
#include "contacts/Link.h"
#include <sys/types.h> 

#include <set>
#include <map>
#include <vector>

#include "routing/ProphetEncounter.h"

#include "bundling/BundleList.h"
#include "bundling/BundleActions.h"

#include "reg/Registration.h"

/**
 * Pages and paragraphs refer to IETF Prophet, March 2006
 */

namespace dtn {

/**
 * A ProphetNode represents the local's relationship with a remote
 * Prophet node, as specified by ProphetNode::remote_eid()
 * <br>
 * ProphetTable represents the set of all Prophet nodes that local
 * has ever encountered, by direct contact or by transitivity.<br>
 * <br>
 * ProphetEncounter represents the active exchange of information
 * between local and ProphetEncounter::remote_; as such, it is the
 * state machine described by the Prophet internet draft (March 2006).
 * Any context-specific information exchanged between nodes (such as
 * RIBD) is kept by ProphetEncounter.  Once the RIB is echanged, 
 * delivery predictability updates take place in ProphetTable, and
 * thus affect all instances of ProphetEncounter.<br>
 * <br>
 * ProphetController is the control piece that interacts with the
 * BundleRouter API.  It keeps a set of active ProphetEncounters
 * as well as the authoritative ProphetTable.<br>
 * <br>
 * The Prophet protocol requires the following primitives from its
 * bundle agent/routing agent interface:<br>
 * <br> 
 * Get Bundle List<br>
 *    the controller keeps a list (@param bundles_)<br>
 * Send Bundle<br>
 *    actions_->send_bundle()<br>
 * Accept Bundle<br>
 *    bundles_->push_back(bundle)<br>
 * Bundle Delivered<br>
 *    BundleDaemon::post(new BundleDeliveredEvent())
 * Drop Bundle<br>
 *    actions_->delete_bundle(bundle)<br>
 *    bundles_->erase(bundle)<br>
 */

class ProphetController : public oasys::Logger,
                          public ProphetOracle,
                          public oasys::Singleton<ProphetController,false>
{
public:
    /**
     * Constructor
     */
    ProphetController();

    /**
     * Initialization function, called by init() at construction time
     */
    void do_init(ProphetParams* params, const BundleList* bundles,
                 BundleActions* actions, const char* logpath);

    /**
     * Factory method (invoked instead of constructor)
     */
    static void init(ProphetParams* params,
                     const BundleList* bundles,
                     BundleActions* actions,
                     const char* logpath = "/dtn/route/prophet/controller")
    {
        ASSERTF(instance_ == NULL,"ProphetController already initialized");
        instance_ = new ProphetController();
        instance_->do_init(params,bundles,actions,logpath);
    }

    virtual ~ProphetController();

    Prophet::fwd_strategy_t fwd_strategy() const { return params_->fs_; }
    void set_fwd_strategy( Prophet::fwd_strategy_t f ) { params_->fs_ = f; }

    Prophet::q_policy_t q_policy() const { return params_->qp_; }
    void set_q_policy( Prophet::q_policy_t q ) { params_->qp_ = q; }

    /**
     * Snarf something intelligent from TableBasedRouter ?
     */
    void dump_state(oasys::StringBuffer*);

    /**
     * Prophet's handler for receiving all Bundles, including 
     * Prophet control messages (TLVs).  For TLVs, demux which
     * ProphetEncounter to deliver to.
     */
    void handle_bundle_received(Bundle*,const ContactRef&);

    /**
     * Handler for bundle expired event
     */
    void handle_bundle_expired(Bundle*);

    /**
     * Handler for bundle delivered event
     */
    void handle_bundle_delivered(Bundle*);

    /**
     * Handler for when a busy link becomes available
     */
    void handle_link_state_change_request(const ContactRef&);
    
    /**
     * New link has come up; attempt to establish contact
     */
    void new_neighbor(const ContactRef&);

    /**
     * Existing link has gone away; clean up state
     */
    void neighbor_gone(const ContactRef&);

    /**
     * Callback to handle queue policy change
     */
    void handle_queue_policy_change(Prophet::q_policy_t qp)
    {
        if (qp != Prophet::INVALID_QP &&
            params_->qp_ != qp)
        {
            log_info("changing queue policy from %s to %s",
                     Prophet::qp_to_str(params_->qp_),
                     Prophet::qp_to_str(qp));
            params_->qp_ = qp;
            bundles_->set_comp(QueueComp::queuecomp(qp,&pstats_,&nodes_));
        }
        else
        {
            log_info("not changing queue_policy (no difference)");
        }
    }

    /**
     * Callback to handle hello_interval change
     */
    void handle_hello_interval_change(u_int hello_interval)
    {
        if (hello_interval != params_->hello_interval_)
        {
            log_info("changing hello_interval from %u to %u",
                     params_->hello_interval_,hello_interval);
            params_->hello_interval_ = hello_interval;
        }
        else
        {
            log_info("not changing hello_interval (no difference)");
        }
        for(enc_set::iterator i = encounters_.begin();
            i != encounters_.end();
            i++)
        {
            ProphetEncounter* pe = *i;
            pe->hello_interval_changed();
        }
    }

    /**
     * Callback to handle max_usage change
     */
    void handle_max_usage_change(u_int max_usage)
    {
        if (max_usage != params_->max_usage_)
        {
            log_info("changing max_usage from %u to %u",
                     params_->max_usage_,max_usage);
            params_->max_usage_ = max_usage;
            bundles_->set_max(max_usage);
        }
        else
        {
            log_info("not changing max_usage (no difference)");
        }
    }

    /**
     * Callback to shutdown ProphetEncounter threads prior to destructor
     */
    void shutdown();

    /// @{ virtual from ProphetOracle
    ProphetParams*      params()  { return params_;  }
    ProphetBundleQueue* bundles() { return bundles_; }
    ProphetTable*       nodes()   { return &nodes_;  }
    BundleActions*      actions() { return actions_; }
    ProphetAckList*     acks()    { return &acks_;   }
    ProphetStats*       stats()   { return &pstats_; }
    /// @}

    bool reg(ProphetEncounter* pe)
    {
        oasys::ScopeLock l(lock_,"reg");
        bool ok = (encounters_.insert(pe).second == true);
        if (ok)
            log_info("ProphetEncounter %d has registered",
                     pe->local_instance());
        return ok;
    }

    bool unreg(ProphetEncounter* pe)
    {
        oasys::ScopeLock l(lock_,"unreg");
        bool ok = (encounters_.erase(pe) == 1);
        if (ok)
            log_info("ProphetEncounter %d has unregistered",
                     pe->local_instance());
        return ok;
    }
    
    static bool is_init() { return instance_ != NULL; } 

    bool accept_bundle(Bundle*,int*);
protected:

    /**
     * Look up and return the delivery predictability (p_value) for
     * this Bundle
     */
    double p_value(Bundle*);

    typedef std::set<ProphetEncounter*> enc_set;

    ProphetEncounter* find_instance(const LinkRef& link);

    /**
     * Prophet parameters, to share among ProphetEncounters
     */
    ProphetParams* params_;

    /**
     * The collection of active sessions with other Prophet nodes
     */
    enc_set encounters_;

    /**
     * Control concurrent access to encounter list
     */
    oasys::SpinLock *lock_;

    /**
     * Authoritative list of all Prophet nodes seen by local node
     */
    ProphetTable nodes_;

    /**
     * Applies aging algorithm to node table
     */
    ProphetTableAgeTimer* node_age_timer_;

    /**
     * Timer to effect Prophet ACK expiration
     */
    ProphetAckAgeTimer* ack_age_timer_;

    /**
     * All bundles ProphetRouter has ACKs for
     */
    ProphetAckList acks_;

    /**
     * Used to track various statistics for forwarding and queuing
     */
    ProphetStats pstats_;

    /**
     * Router's bundle action object
     */
    BundleActions* actions_;

    /**
     * Bounded Priority Queue with queuing policy ordering
     */
    ProphetBundleQueue* bundles_;

    /**
     * EndpointID of this Prophet instance
     */
    EndpointID prophet_eid_;

}; // ProphetController

}; // dtn

#endif // _DTN_PROPHET_CONTROLLER_

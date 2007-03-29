/*
 *    Copyright 2006 Intel Corporation
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

#ifndef _FORWARDINGINFO_H_
#define _FORWARDINGINFO_H_

#include <string>
#include <sys/time.h>
#include <oasys/serialize/Serialize.h>
#include "CustodyTimer.h"

namespace dtn {

/**
 * Class to encapsulate bundle forwarding information. This is created
 * when a bundle is forwarded to log a record of the forwarding event,
 * along with any route-specific information about the action, such as
 * the custody timer.
 *
 * Routing algorithms consult this log to determine their course of
 * action, for instance if they don't want to retransmit to the same
 * next hop twice.
 */
class ForwardingInfo : public oasys::SerializableObject{
public:
    /**
     * The forwarding action type codes.
     */
    typedef enum {
        INVALID_ACTION = 0,	///< Invalid action
        FORWARD_ACTION,		///< Forward the bundle to only this next hop
        COPY_ACTION		///< Forward a copy of the bundle
    } action_t;

    static inline const char* action_to_str(action_t action)
    {
        switch(action) {
        case INVALID_ACTION:	return "INVALID";
        case FORWARD_ACTION:	return "FORWARD";
        case COPY_ACTION:	return "COPY";
        default:
            NOTREACHED;
        }
    }

    /**
     * The forwarding log state codes.
     */
    typedef enum {
        NONE,             ///< Return value for no entry
        IN_FLIGHT,        ///< Currently being sent
        TRANSMITTED,      ///< Successfully transmitted
        TRANSMIT_FAILED,  ///< Transmission failed
        CANCELLED,	  ///< Transmission cancelled
        CUSTODY_TIMEOUT,  ///< Custody transfer timeout
    } state_t;

    static const char* state_to_str(state_t state)
    {
        switch(state) {
        case NONE:      	return "NONE";
        case IN_FLIGHT: 	return "IN_FLIGHT";
        case TRANSMITTED:      	return "TRANSMITTED";
        case TRANSMIT_FAILED:  	return "TRANSMIT_FAILED";
        case CANCELLED: 	return "CANCELLED";
        case CUSTODY_TIMEOUT:	return "CUSTODY_TIMEOUT";
        default:
            NOTREACHED;
        }
    }

    /**
     * Default constructor.
     */
    ForwardingInfo()
        : state_(NONE),
          action_(INVALID_ACTION),
          clayer_(""),
          nexthop_(""),
          remote_eid_(""),
          link_name_(""),
          custody_timer_() {}

    /*
     * Constructor for serialization.
     */
    ForwardingInfo(const oasys::Builder&)
        : state_(NONE),
          action_(INVALID_ACTION),
          clayer_(""),
          nexthop_(""),
          remote_eid_(""),
          link_name_(""),
          custody_timer_() {}
    
    /**
     * Constructor used for new entries.
     */
    ForwardingInfo(state_t state,
                   action_t action,
                   const std::string& clayer,
                   const std::string& nexthop,
                   const std::string& remote_eid,
                   const std::string& link_name,
                   const CustodyTimerSpec& custody_timer)
        : state_(NONE),
          action_(action),
          clayer_(clayer),
          nexthop_(nexthop),
          remote_eid_(remote_eid),
          link_name_(link_name),
          custody_timer_(custody_timer)
    {
        set_state(state);
    }

    /**
     * Set the state and update the timestamp.
     */
    void set_state(state_t new_state)
    {
        state_ = new_state;
        ::gettimeofday(&timestamp_, 0);
    }

    virtual void serialize( oasys::SerializeAction *a );
    int              state_;		///< State of the transmission
    int              action_;           ///< Forwarding action
    std::string      clayer_;		///< Convergence layer for the contact
    									///< this should not change
    std::string      nexthop_;		///< CL-specific nexthop string
    									///< A link's nexthop can change	
    std::string      remote_eid_;	///< the EID of target of this link
    									///< this should not change
    std::string      link_name_;    ///< The name of the link object
    									///< this should not change
    struct timeval   timestamp_;	///< Timestamp of last state update
    CustodyTimerSpec custody_timer_;	///< Custody timer information 
};

} // namespace dtn

#endif /* _FORWARDINGINFO_H_ */

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
#include <oasys/util/Time.h>
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
class ForwardingInfo : public oasys::SerializableObject {
public:
    /**
     * The forwarding action type codes.
     */
    typedef enum {
        INVALID_ACTION = 0,	///< Invalid action
        FORWARD_ACTION = 1 << 0,///< Forward the bundle to only this next hop
        COPY_ACTION    = 1 << 1,///< Forward a copy of the bundle
    } action_t;
    
    /**
     * Convenience flag to specify any forwarding action for use in
     * searching the log.
     */
    static const unsigned int ANY_ACTION = 0xffffffff;

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
        NONE             = 0,      ///< Return value for no entry
        IN_FLIGHT        = 1 << 0, ///< Currently being sent
        TRANSMITTED      = 1 << 1, ///< Successfully transmitted
        TRANSMIT_FAILED  = 1 << 2, ///< Transmission failed
        CANCELLED        = 1 << 3, ///< Transmission cancelled
        CUSTODY_TIMEOUT  = 1 << 4, ///< Custody transfer timeout
        TRANSMIT_PENDING = 1 << 5, ///< Waiting for link availability
    } state_t;

    /**
     * Convenience flag to specify any forwarding state for use in
     * searching the log.
     */
    static const unsigned int ANY_STATE = 0xffffffff;

    static const char* state_to_str(state_t state)
    {
        switch(state) {
        case NONE:      	return "NONE";
        case IN_FLIGHT: 	return "IN_FLIGHT";
        case TRANSMITTED:      	return "TRANSMITTED";
        case TRANSMIT_FAILED:  	return "TRANSMIT_FAILED";
        case CANCELLED: 	return "CANCELLED";
        case CUSTODY_TIMEOUT:	return "CUSTODY_TIMEOUT";
        case TRANSMIT_PENDING:	return "TRANSMIT_PENDING";
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
          link_name_(""),
          remote_eid_(),
          custody_spec_() {}

    /*
     * Constructor for serialization.
     */
    ForwardingInfo(const oasys::Builder& builder)
        : state_(NONE),
          action_(INVALID_ACTION),
          link_name_(""),
          remote_eid_(builder),
          custody_spec_() {}
    
    /**
     * Constructor used for new entries.
     */
    ForwardingInfo(state_t state,
                   action_t action,
                   const std::string& link_name,
                   const EndpointID& remote_eid,
                   const CustodyTimerSpec& custody_spec)
        : state_(NONE),
          action_(action),
          link_name_(link_name),
          remote_eid_(remote_eid),
          custody_spec_(custody_spec)
    {
        set_state(state);
    }

    /**
     * Set the state and update the timestamp.
     */
    void set_state(state_t new_state)
    {
        state_ = new_state;
        timestamp_.get_time();
    }

    virtual void serialize(oasys::SerializeAction *a);

    /// @{ Accessors
    const state_t  state()  const { return static_cast<state_t>(state_); }
    const action_t action() const { return static_cast<action_t>(action_); }
    const std::string&      link_name()    const { return link_name_; }
    const EndpointID&       remote_eid()   const { return remote_eid_; }
    const oasys::Time&      timestamp()    const { return timestamp_; }
    const CustodyTimerSpec& custody_spec() const { return custody_spec_; }
    /// @}
    
private:
    u_int32_t        state_;            ///< State of the transmission
    u_int32_t        action_;           ///< Forwarding action
    std::string      link_name_;        ///< The name of the link
    EndpointID       remote_eid_;       ///< The EID of the next hop node
    oasys::Time      timestamp_;        ///< Timestamp of last state update
    CustodyTimerSpec custody_spec_;     ///< Custody timer information 
};

} // namespace dtn

#endif /* _FORWARDINGINFO_H_ */

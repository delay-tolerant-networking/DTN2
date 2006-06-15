/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2006 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _FORWARDINGINFO_H_
#define _FORWARDINGINFO_H_

#include <string>
#include <sys/time.h>
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
class ForwardingInfo {
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
          custody_timer_() {}
    
    /**
     * Constructor used for new entries.
     */
    ForwardingInfo(state_t state,
                   action_t action,
                   const std::string& clayer,
                   const std::string& nexthop,
                   const CustodyTimerSpec& custody_timer)
        : state_(NONE),
          action_(action),
          clayer_(clayer),
          nexthop_(nexthop),
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
    
    state_t          state_;		///< State of the transmission
    action_t	     action_;           ///< Forwarding action
    std::string      clayer_;		///< Convergence layer for the contact
    std::string      nexthop_;		///< CL-specific nexthop string
    struct timeval   timestamp_;	///< Timestamp of last state update
    CustodyTimerSpec custody_timer_;	///< Custody timer information 
};

} // namespace dtn

#endif /* _FORWARDINGINFO_H_ */

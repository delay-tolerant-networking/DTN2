/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 *
 * Intel Open Source License
 *
 * Copyright (c) 2005 Intel Corporation. All rights reserved.
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
#ifndef _FORWARDINGLOG_H_
#define _FORWARDINGLOG_H_

#include <vector>

#include "ForwardingInfo.h"

namespace oasys {
class SpinLock;
class StringBuffer;
}

namespace dtn {

class Link;
class ForwardingLog;

/**
 * Class to maintain a log of informational records as to where and
 * when a bundle has been forwarded.
 *
 * This state can be (and is) used by the router logic to prevent it
 * from naively forwarding a bundle to the same next hop multiple
 * times. It also is used to store the custody timer spec as supplied
 * by the router so the main forwarding engine knows how to set the
 * appropriate timer.
 *
 * Although a bundle can be sent over multiple links, and may even be
 * sent over the same link multiple times, the forwarding logic
 * assumes that for a given link and bundle, there is only one active
 * transmission. Thus the accessors below always return / update the
 * last entry in the log for a given link.
 */
class ForwardingLog {
public:
    typedef ForwardingInfo::state_t state_t;

    /**
     * Constructor that takes a pointer to the relevant Bundle's lock,
     * used when querying or updating the log.
     */
    ForwardingLog(oasys::SpinLock* lock);
    
    /**
     * Get the most recent entry for the given link from the log.
     */
    bool get_latest_entry(Link* link, ForwardingInfo* info) const;
    
    /**
     * Get the most recent state for the given link from the log.
     */
    state_t get_latest_entry(Link* link) const;
    
    /**
     * Return the transmission count of the bundle, optionally
     * including inflight entries as well. If an action is specified
     * (i.e. not FORWARD_INVALID), only count log entries that match
     * the action code.
     */
    size_t get_transmission_count(bundle_fwd_action_t action = FORWARD_INVALID,
                                  bool include_inflight = false) const;
    
    /**
     * Add a new forwarding info entry for the given link.
     */
    void add_entry(Link* link,
                   bundle_fwd_action_t action,
                   state_t state,
                   const CustodyTimerSpec& custody_timer);
    
    /**
     * Update the state for the latest forwarding info entry for the
     * given link.
     *
     * @return true if the next hop entry was found
     */
    bool update(Link* link, state_t state);
    
    /**
     * Return a count of the number of entries in the given state.
     */
    size_t get_count(state_t state) const;

    /**
     * Dump a string representation of the log.
     */
    void dump(oasys::StringBuffer* buf) const;

    /**
     * Typedef for the log itself.
     */
    typedef std::vector<ForwardingInfo> Log;

protected:
    Log log_;			///< The actual log
    oasys::SpinLock* lock_;	///< Copy of the bundle's lock
};

} // namespace dtn


#endif /* _FORWARDINGLOG_H_ */

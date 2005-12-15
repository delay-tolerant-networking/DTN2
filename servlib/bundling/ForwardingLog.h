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

#include <string>
#include <vector>
#include <sys/time.h>

#include <oasys/debug/DebugUtils.h>

namespace dtn {

class ForwardingEntry;
class ForwardingLog;

/**
 * Class to encapsulate a single entry in the forwarding log.
 */
class ForwardingEntry {
public:
    typedef enum {
        NONE            = 0, ///< Return value for no entry
        IN_FLIGHT       = 1, ///< Currently being sent
        SENT            = 2, ///< Successfully sent
        CANCELLED	= 4, ///< Transmission cancelled
    } state_t;

    static const char* state_to_str(state_t state)
    {
        switch(state) {
        case NONE:      return "NONE";
        case IN_FLIGHT: return "IN_FLIGHT";
        case SENT:      return "SENT";
        case CANCELLED: return "CANCELLED";
        default:
            NOTREACHED;
        }
    }

    ForwardingEntry(const std::string& nexthop, state_t state)
        : nexthop_(nexthop), state_(state) {}

    std::string    nexthop_;
    state_t        state_;
    struct timeval timestamp_;
};

/**
 * Class to maintain a log of next hop addresses where a bundle has
 * been forwarded.
 *
 * Used by the router to make sure that a bundle isn't forwarded to
 * the same next hop multiple times.
 */
class ForwardingLog : public std::vector<ForwardingEntry> {
public:
    typedef ForwardingEntry::state_t state_t;

    /**
     * Get the state for the given next hop from the log
     */
    state_t get_state(const std::string& nexthop) const;

    /**
     * Update or add a new entry for the given nexthop.
     */
    void update(const std::string& nexthop, state_t state);
};

} // namespace dtn


#endif /* _FORWARDINGLOG_H_ */

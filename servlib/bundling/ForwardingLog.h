/*
 *    Copyright 2005-2006 Intel Corporation
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

#ifndef _FORWARDINGLOG_H_
#define _FORWARDINGLOG_H_

#include <vector>

#include <oasys/serialize/SerializableVector.h>
#include "ForwardingInfo.h"
#include "contacts/Link.h"

namespace oasys {
class SpinLock;
class StringBuffer;
}

namespace dtn {

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
class ForwardingLog : public oasys::SerializableVector<ForwardingInfo>{
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
    bool get_latest_entry(const LinkRef& link, ForwardingInfo* info) const;
    
    /**
     * Get the most recent state for the given link from the log.
     */
    state_t get_latest_entry(const LinkRef& link) const;
    
    /**
     * Return the transmission count of the bundle, optionally
     * including inflight entries as well. If an action is specified
     * (i.e. not ForwardingInfo::INVALID_ACTION), only count log entries
     * that match the action code.
     */
    size_t get_transmission_count(ForwardingInfo::action_t action =
                                    ForwardingInfo::INVALID_ACTION,
                                  bool include_inflight = false) const;
    
    /**
     * Add a new forwarding info entry for the given link.
     */
    void add_entry(const LinkRef& link,
                   ForwardingInfo::action_t action,
                   state_t state,
                   const CustodyTimerSpec& custody_timer);
    
    /**
     * Update the state for the latest forwarding info entry for the
     * given link.
     *
     * @return true if the next hop entry was found
     */
    bool update(const LinkRef& link, state_t state);
    
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

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
#ifndef _CONTACT_MANAGER_H_
#define _CONTACT_MANAGER_H_

#include <oasys/debug/Log.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/StringUtils.h>
#include "bundling/BundleEventHandler.h"

namespace dtn {

class Contact;
class ConvergenceLayer;
class CLInfo;
class Link;
class LinkSet;

/**
 * A contact manager class. Maintains topological information and
 * connectivity state regarding available links and contacts.
 */
class ContactManager : public BundleEventHandler {
public:
    /**
     * Constructor / Destructor
     */
    ContactManager();
    virtual ~ContactManager();
    
    /**
     * Dump a string representation of the info inside contact manager.
     */
    void dump(oasys::StringBuffer* buf) const;
    
    /**********************************************
     *
     * Link set accessor functions
     *
     *********************************************/
    /**
     * Add a link.
     */
    void add_link(Link* link);
    
    /**
     * Delete a link
     */
    void del_link(Link* link);
    
    /**
     * Check if contact manager already has this link
     */
    bool has_link(Link* link);

    /**
     * Finds link corresponding to this name
     */
    Link* find_link(const char* name);

    /**
     * Helper routine to find a link based on the given criteria
     *
     * @param cl 	 The convergence layer
     * @param nexthop	 The next hop string
     * @param remote_eid Remote endpoint id
     * @param type	 Link type (LINK_INVALID for any)
     * @param states	 Bit vector of legal link states, e.g. ~(OPEN | OPENING)
     *
     * @return The link if it matches or NULL if there's no match
     */
    Link* find_link_to(ConvergenceLayer* cl,
                       const std::string& nexthop,
                       const EndpointID& remote_eid,
                       Link::link_type_t type,
                       u_int states);
    
    /**
     * Finds link to given next hop. Optionally restricts the search
     * to include only links using the given convergence layer.
     *
     * XXX/demmer should this include remote_eid as well?
     */
    Link* find_link_to(const char* next_hop, const char* clayer = NULL);

    /**
     * Return the list of links. Asserts that the CM spin lock is held
     * by the caller.
     */
    const LinkSet* links();

    /**
     * Accessor for the ContactManager internal lock.
     */
    oasys::Lock* lock() { return &lock_; }

    /**********************************************
     *
     * Event handler routines
     *
     *********************************************/
    /**
     * Generic event handler.
     */
    void handle_event(BundleEvent* event)
    {
        dispatch_event(event);
    }
    
    /**
     * Event handler when a link becomes unavailable.
     */
    void handle_link_available(LinkAvailableEvent* event);
    
    /**
     * Event handler when a link becomes unavailable.
     */
    void handle_link_unavailable(LinkUnavailableEvent* event);

    /**
     * Event handler when a link is opened successfully
     */
    void handle_contact_up(ContactUpEvent* event);

    /**********************************************
     *
     * Opportunistic contact routines
     *
     *********************************************/
    /**
     * Notification from a convergence layer that a new opportunistic
     * link has come knocking.
     *
     * @return An idle link to represent the new contact
     */
    Link* new_opportunistic_link(ConvergenceLayer* cl,
                                 const std::string& nexthop,
                                 const EndpointID& remote_eid);
    
protected:
    
    LinkSet* links_;			///< Set of all links
    int opportunistic_cnt_;		///< Counter for opportunistic links

    /**
     * Reopen a broken link.
     */
    void reopen_link(Link* link);

    /**
     * Timer class used to re-enable broken ondemand links.
     */
    class LinkAvailabilityTimer : public oasys::Timer {
    public:
        LinkAvailabilityTimer(ContactManager* cm, Link* link)
            : cm_(cm), link_(link) {}
        
        virtual void timeout(const struct timeval& now);

        ContactManager* cm_;	///< The contact manager object
        Link* link_;		///< The link in question
    };
    friend class LinkAvailabilityTimer;

    /**
     * Table storing link -> availability timer class.
     */
    typedef std::map<Link*, LinkAvailabilityTimer*> AvailabilityTimerMap;
    AvailabilityTimerMap availability_timers_;

    /**
     * Lock to protect internal data structures.
     */
    mutable oasys::SpinLock lock_;
};


} // namespace dtn

#endif /* _CONTACT_MANAGER_H_ */

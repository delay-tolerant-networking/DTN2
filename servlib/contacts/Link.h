/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
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
#ifndef _LINK_H_
#define _LINK_H_

#include <set>
#include <oasys/debug/Formatter.h>

#include "BundleConsumer.h"
#include "BundleTuple.h"
#include "BundleList.h"

namespace dtn {

class ConvergenceLayer;
class CLInfo;
class Contact;
class Link;

/**
 * Set of links
 */
class LinkSet : public std::set<Link*> {};

/**
 * Abstraction for a DTN link. A DTN link has a destination next hop
 * associated with it.
 
 * The state of a link (regarding its availability) is described by
 * two bits: First: Every link has a 'bool' availability bit. This
 * indicates whether the link is a potential candidate to be used for
 * actual transmission of packets. If this bit is set the link can be
 * used to send packets, and if not then it can not be used to send
 * packets. This bit is also the key difference between different
 * types of links. How is the bit set/unset is discussed later. (see
 * below)
 *
 * Second: A link can have a contact associated with it. A contact
 * represents an actual connection. A contact is created only if
 * link's availability is set. If a contact exists on link we say that
 * the link is open (i.e contact_ != NULL), otherwise we say that the
 * link is closed.
 *
 * Every link has a unique name asssociated with it which is used to
 * identify it. The name is configured explicitly when the link is
 * created.
 *
 * Creating new links:
 * Links are created explicitly in the configuration. Syntax is:
 * @code
 * link add <name> <nexthop> <type> <conv_layer> <args>
 * @endcode
 * See servlib/cmd/LinkCommand.cc for implementation of this TCL cmd.
 *
 *  ----------------------------------------------------------
 *
 * Invariants for better understanding: Opening a link means =>
 * Creating a new contact on it. It is an error to open a link which
 * is already open. One can check status, if the link is already open
 * or not. Links are typically opened by the bundle actions layer
 * router when . A Link can be opened only if its
 * availability bit is set to true
 *
 * Closing a link means => Deleting or getting rid of existing contact
 * on it. Free the state stored for that contact. It is an error to
 * close a link which is already closed. The link close procedure is
 * always issued in response to a CONTACT_DOWN event being sent to the
 * router. This may be in response to a user command or the scheduling
 * layer, or from the convergence layer itself in response to a
 * timeout or other such event.
 *
 * XXX/demmer update above
 *
 * ----------------------------------------------------------
 *
 * Links are of three types as discussed in the DTN architecture
 * ONDEMAND, SCHEDULED, OPPORTUNISTIC.
 *
 * The key differences from an implementation perspectives are
 * "who" and "when"  sets/unsets the link availability bit.
 *
 * Only for ONDEMAND links is the availability bit is set when the
 * link is constructed. i.e an ONDEMAND link is always available as
 * one would expect. Though by default it is not open until the router
 * explicitly opens it.
 *
 * An ONDEMAND link can be closed either because of convergence layer
 * failure or because the link has been idle for too long. If link
 * gets closed because the convergence layer detects failure and there
 * are messages which are queued on the link, the router will
 * automatically open the link again.
 *
 * For ONDEMAND links mostly in the default router implemetation there
 * are no messages queued on the link queue. This is because since
 * link is always available as soon as something is to be queued on
 * the link router tries to open it. And once the link is open all
 * messages are queued on the contact queue. If the convergence layer
 * closes the link because of failure, there may be messages queued on
 * the link temporarily before the link is opened again.
 *
 *
 * For OPPORTUNISTIC links the availability bit is set by the code which
 * detects when the link is available to be used.
 * The OPPORTUNISTIC link has a queue containing bundles which are waiting 
 * for link to become available. When the link becomes available the
 * router is informed.
 * It is up to the router to open a new contact on the link and transfer 
 * messages from one queue to another. Similarly when a contact is broken
 * the router performs the transferring of messages from contact's
 * queue to link's queue.
 * The key is that an external entity controls when to set/unset link
 * availability bit.
 * ONDEMAND links may also store history or any other aggregrate
 * statistics in link info if they need.
 *
 * SCHEDULED links
 *
 * Scheduled links have a list of future contacts.
 * A future contact is nothing more than a place to store messages and
 * some associated time at which the contact is supposed to happen.
 * By definition, future contacts are not active entities. Based
 * on schedule they are converted from future contact to contact
 * when they become active entity. Therefore, the way contacts
 * are created for scheduled links is to convert an existing future
 * contact into a current contact.
 * How and when are future contacts created is an open question.
 *
 * A scheduled link can also have its own queue. This happens
 * when router does not want to assign bundle to any specific
 * future contact. When an actual contact comes / or rather a future
 * contact becomes a contact all the data should be transferred from
 * the link queue to the contact queue.
 * (similar to what happens in OPPORTUNISTIC links)
 *
 * An interesting question is to define the order in which the messages
 * from the link queue and the contact queue are merged. This is because
 * router may already have scheduled some messages on the queue of
 * current contact when it was a future contact.
 *
 */
class Link : public oasys::Formatter, public QueueConsumer {
public:
    /**
     * Valid types for a link.
     */
    typedef enum
    {
        LINK_INVALID = -1,
        
        /**
         * The link is expected to be either ALWAYS available, or can
         * be made available easily. Examples include DSL (always),
         * and dialup (easily available).
         */
        ONDEMAND = 1,
        
        /**
         * The link is only available at pre-determined times.
         */
        SCHEDULED = 2,
        
        /**
         * The link may or may not be available, based on
         * uncontrollable factors. Examples include a wireless link
         * whose connectivity depends on the relative locations of the
         * two nodes.
         */
        OPPORTUNISTIC = 3
    }
    link_type_t;

    /**
     * Link type string conversion.
     */
    static inline const char*
    link_type_to_str(link_type_t type)
    {
        switch(type) {
        case ONDEMAND:		return "ONDEMAND";
        case SCHEDULED:		return "SCHEDULED";
        case OPPORTUNISTIC: 	return "OPPORTUNISTIC";
        default: 		PANIC("bogus link_type_t");
        }
    }

    static inline link_type_t
    str_to_link_type(const char* str)
    {
        if (strcasecmp(str, "ONDEMAND") == 0)
            return ONDEMAND;
        
        if (strcasecmp(str, "SCHEDULED") == 0)
            return SCHEDULED;
        
        if (strcasecmp(str, "OPPORTUNISTIC") == 0)
            return OPPORTUNISTIC;
        
        return LINK_INVALID;
    }
    
    /**
     * Static function to create appropriate link object from link type.
     */
    static Link* create_link(std::string name, link_type_t type,
                             ConvergenceLayer* cl, const char* nexthop,
                             int argc, const char* argv[]);
    /**
     * Constructor / Destructor
     */
    Link(std::string name, link_type_t type,
         ConvergenceLayer* cl, const char* nexthop);
    virtual ~Link();

    /**
     * Return the type of the link.
     */
    link_type_t type() { return type_; }

    /**
     * Return the string for of the link.
     */
    const char* type_str() { return link_type_to_str(type_); }

    /**
     * Return the link's current contact
     */
    Contact* contact() { return contact_; }
    
    /**
     * Return the state of the link.
     */
    bool isopen() { return contact_ != NULL; }

    /**
     * Return the state of the link.
     */
    bool isavailable() { return avail_; }

    /**
     * Return whether the link is in the process of shutting down.
     */
    bool isclosing() { return closing_; }

    /**
     * Set the state of the link to be available
     */
    void set_link_available();

    /**
     * Set the state of the link to be unavailable
     */
    void set_link_unavailable();

    /**
     * Find how many messages are queued to go through this link
     */
    size_t size();
    
    /// @{
    /// Virtual from BundleConsumer / QueueConsumer
    virtual void consume_bundle(Bundle* bundle, const BundleMapping* mapping);
    virtual bool dequeue_bundle(Bundle* bundle, BundleMapping** mappingp);
    virtual bool is_queued(Bundle* bundle);
    /// @}
    
    /**
     * Store convergence layer state associated with the link.
     */
    void set_cl_info(CLInfo* cl_info)
    {
        ASSERT((cl_info_ == NULL && cl_info != NULL) ||
               (cl_info_ != NULL && cl_info == NULL));
        
        cl_info_ = cl_info;
    }

    /**
     * Accessor to the convergence layer state.
     */
    CLInfo* cl_info() { return cl_info_; }
    
    /**
     * Accessor to this contact's convergence layer.
     */
    ConvergenceLayer* clayer() { return clayer_; }

    /**
     * Accessor to this links name
     */
    const char* name() { return name_.c_str(); }

    /**
     * Accessor to next hop string
     */
    const char* nexthop() { return nexthop_.c_str(); }

    /**
     * Virtual from formatter
     */
    int format(char* buf, size_t sz);
    
protected:
    friend class BundleActions;
    friend class ContactManager;
    
    /**
     * Open/Close link
     * It is protected to not prevent random system
     * components call open/close
     */
    virtual void open();
    virtual void close();

    /// Type of the link
    link_type_t type_;

    /// Associated next hop admin identifier
    std::string nexthop_;
    
    /// Name of the link to identify across daemon
    std::string name_;

    /// Current contact. contact_ != null iff link is open
    Contact* contact_;

    /// Availability bit
    bool avail_;

    /// Close-in-progress bit
    bool closing_;

    /// Pointer to convergence layer
    ConvergenceLayer* clayer_;

    /// Convergence layer specific info, if needed
    CLInfo* cl_info_;
};

} // namespace dtn

#endif /* _LINK_H_ */

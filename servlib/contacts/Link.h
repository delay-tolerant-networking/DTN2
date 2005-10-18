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

#include "bundling/BundleConsumer.h"
#include "bundling/BundleList.h"

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
 * Abstraction for a DTN link, i.e. a one way communication channel to
 * a next hop node in the DTN overlay.
 *
 * The state of a link (regarding its availability) is described by
 * the Link::state_t enumerated type.
 *
 * All links in the OPEN, BUSY, or CLOSING states have an associated
 * contact that represents an actual connection.
 *
 * Every link has a unique name asssociated with it which is used to
 * identify it. The name is configured explicitly when the link is
 * created.
 *
 * Creating new links:
 * Links are created explicitly in the configuration file. Syntax is:
 * @code
 * link add <name> <nexthop> <type> <conv_layer> <args>
 * @endcode
 * See servlib/cmd/LinkCommand.cc for implementation of this TCL cmd.
 *
 * ----------------------------------------------------------
 *
 * Links are of three types as discussed in the DTN architecture
 * ONDEMAND, SCHEDULED, OPPORTUNISTIC.
 *
 * The key differences from an implementation perspectives are "who"
 * and "when" manipulates the link state regarding availability.
 *
 * ONDEMAND links are initializd in the AVAILABLE state, as one would
 * expect. It remains in this state until a router explicitly opens
 * it.
 *
 * An ONDEMAND link can then be closed either due to connection
 * failure or because the link has been idle for too long, both
 * triggered by the convergence layer. If an ONDEMAND link is closed
 * due to connection failure, then the contact manager is notified of
 * this event and periodically tries to re-establish the link.
 *
 * For OPPORTUNISTIC links the availability state is set by the code
 * which detects that there is a new link available to be used. 
 *
 * SCHEDULED links have their availability dictated by the schedule
 * implementation.
 */
class Link : public oasys::Formatter, public BundleConsumer {
public:
    /**
     * Valid types for a link.
     */
    typedef enum
    {
        LINK_INVALID = -1,
        
        /**
         * The link is expected to be ALWAYS available, and any
         * convergence layer connection state is always maintained for
         * it.
         */
        ALWAYSON = 1,
        
        /**
         * The link is expected to be either always available, or can
         * be made available easily. Examples include DSL (always),
         * and dialup (easily available). Convergence layers are free
         * to tear down idle connection state, but are expected to be
         * able to easily re-establish it.
         */
        ONDEMAND = 2,
        
        /**
         * The link is only available at pre-determined times.
         */
        SCHEDULED = 3,
        
        /**
         * The link may or may not be available, based on
         * uncontrollable factors. Examples include a wireless link
         * whose connectivity depends on the relative locations of the
         * two nodes.
         */
        OPPORTUNISTIC = 4
    }
    link_type_t;

    /**
     * Link type string conversion.
     */
    static inline const char*
    link_type_to_str(link_type_t type)
    {
        switch(type) {
        case ALWAYSON:		return "ALWAYSON";
        case ONDEMAND:		return "ONDEMAND";
        case SCHEDULED:		return "SCHEDULED";
        case OPPORTUNISTIC: 	return "OPPORTUNISTIC";
        default: 		PANIC("bogus link_type_t");
        }
    }

    static inline link_type_t
    str_to_link_type(const char* str)
    {
        if (strcasecmp(str, "ALWAYSON") == 0)
            return ALWAYSON;
        
        if (strcasecmp(str, "ONDEMAND") == 0)
            return ONDEMAND;
        
        if (strcasecmp(str, "SCHEDULED") == 0)
            return SCHEDULED;
        
        if (strcasecmp(str, "OPPORTUNISTIC") == 0)
            return OPPORTUNISTIC;
        
        return LINK_INVALID;
    }

    /**
     * The possible states for a link.
     */
    typedef enum {
        UNAVAILABLE,	///< The link is closed and not able to be
                        ///  opened currently.

        AVAILABLE,	///< The link is closed but is able to be
                        ///  opened, either because it is an on demand
                        ///  link, or because an opportunistic peer
                        ///  node is in close proximity but no
                        ///  convergence layer session has yet been
                        ///  opened.
        
        OPENING,	///< A convergence layer session is in the
                        ///  process of being established.
        
        OPEN,		///< A convergence layer session has been
                        ///  established, and the link has capacity
                        ///  for a bundle to be sent on it. This may
                        ///  be because no bundle is currently being
                        ///  sent, or because the convergence layer
                        ///  can handle multiple simultaneous bundle
                        ///  transmissions.
        
        BUSY,		///< The link is busy, i.e. a bundle is
                        ///  currently being sent on it by the
                        ///  convergence layer and no more bundles may
                        ///  be delivered to the link.

        CLOSING		///< The link is in the process of being
                        ///  closed.
        
    } state_t;

    /**
     * Convert a link state into a string.
     */
    static inline const char*
    state_to_str(state_t state)
    {
        switch(state) {
        case UNAVAILABLE: 	return "UNAVAILABLE";
        case AVAILABLE: 	return "AVAILABLE";
        case OPENING: 		return "OPENING";
        case OPEN: 		return "OPEN";
        case BUSY: 		return "BUSY";
        case CLOSING: 		return "CLOSING";
        }

        NOTREACHED;
    }
    
    /**
     * Static function to create appropriate link object from link type.
     */
    static Link* create_link(const std::string& name, link_type_t type,
                             ConvergenceLayer* cl, const char* nexthop,
                             int argc, const char* argv[]);
    /**
     * Constructor / Destructor
     */
    Link(const std::string& name, link_type_t type,
         ConvergenceLayer* cl, const char* nexthop);

    /**
     * Hook for subclass to parse arguments.
     */
    virtual int parse_args(int argc, const char* argv[]);
    
    /**
     * Return the type of the link.
     */
    link_type_t type() { return type_; }

    /**
     * Return the string for of the link.
     */
    const char* type_str() { return link_type_to_str(type_); }

    /**
     * Return whether or not the link is open.
     */
    bool isopen() { return ( (state_ == OPEN) ||
                             (state_ == BUSY) ||
                             (state_ == CLOSING) ); }

    /**
     * Return the availability state of the link.
     */
    bool isavailable() { return (state_ != UNAVAILABLE); }

    /**
     * Return the busy state of the link.
     */
    bool isbusy() { return (state_ == BUSY); }

    /**
     * Return whether the link is in the process of shutting down.
     */
    bool isopening() { return (state_ == OPENING); }
    
    /**
     * Return whether the link is in the process of shutting down.
     */
    bool isclosing() { return (state_ == CLOSING); }

    /**
     * Return the actual state.
     */
    state_t state() { return state_; }

    /**
     * Sets the state of the link. Performs various assertions to
     * ensure the state transitions are legal.
     */
    void set_state(state_t state);

    /// @{
    /// Virtual from BundleConsumer / QueueConsumer
    virtual void consume_bundle(Bundle* bundle);
    virtual bool cancel_bundle(Bundle* bundle);
    virtual bool is_queued(Bundle* bundle);
    /// @}
    
    /**
     * Return the current contact information (if any).
     */
    Contact* contact() const { return contact_; }

    /**
     * Set the contact information (used by opportunistic connection
     * CLs).
     */
    void set_contact(Contact* contact)
    {
        ASSERT(contact_ == NULL);
        contact_ = contact;
    }

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
    CLInfo* cl_info() const { return cl_info_; }
    
    /**
     * Accessor to this contact's convergence layer.
     */
    ConvergenceLayer* clayer() const { return clayer_; }

    /**
     * Accessor to this links name
     */
    const char* name() const { return name_.c_str(); }

    /**
     * Accessor to this links name as a c++ string
     */
    const std::string& name_str() const { return name_; }

    /**
     * Accessor to next hop string
     */
    const char* nexthop() const { return nexthop_.c_str(); }

    /**
     * Virtual from formatter
     */
    int format(char* buf, size_t sz) const;
    
protected:
    friend class BundleActions;
    friend class BundleDaemon;
    
    /**
     * Open the link. Protected to make sure only the friend
     * components can call it and virtual to allow subclasses to
     * override it.
     */
    virtual void open();
    
    /**
     * Close the link. Protected to make sure only the friend
     * components can call it and virtual to allow subclasses to
     * override it.
     */
    virtual void close();

    /// Type of the link
    link_type_t type_;

    /// State of the link
    state_t state_;

    /// Next hop address
    std::string nexthop_;
    
    /// Internal name ame of the link 
    std::string name_;

    /// Current contact. contact_ != null iff link is open
    Contact* contact_;

    /// Pointer to convergence layer
    ConvergenceLayer* clayer_;

    /// Convergence layer specific info, if needed
    CLInfo* cl_info_;

    /// Destructor -- protected since links shouldn't be deleted
    virtual ~Link();
};

} // namespace dtn

#endif /* _LINK_H_ */

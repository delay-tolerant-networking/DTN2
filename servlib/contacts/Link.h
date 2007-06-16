/*
 *    Copyright 2004-2006 Intel Corporation
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

#ifndef _LINK_H_
#define _LINK_H_

#include <set>
#include <oasys/debug/Formatter.h>
#include <oasys/serialize/Serialize.h>
#include <oasys/thread/SpinLock.h>
#include <oasys/util/Ref.h>
#include <oasys/util/RefCountedObject.h>

#include "bundling/BundleList.h"
#include "naming/EndpointID.h"

#include "Contact.h"
#include "NamedAttribute.h"

namespace dtn {

class ConvergenceLayer;
class CLInfo;
class Contact;
class Link;

/**
 * Typedef for a reference on a link.
 */
typedef oasys::Ref<Link> LinkRef;

/**
 * Set of links
 */
class LinkSet : public std::set<LinkRef> {};

/**
 * Abstraction for a DTN link, i.e. a one way communication channel to
 * a next hop node in the DTN overlay.
 *
 * The state of a link (regarding its availability) is described by
 * the Link::state_t enumerated type.
 *
 * All links in the OPEN or BUSY states have an associated contact
 * that represents an actual connection.
 *
 * Every link has a unique name associated with it which is used to
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
 * The key differences from an implementation perspective are "who"
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
 *
 * ----------------------------------------------------------
 *
 * Links are used for input and/or output. In other words, for
 * connection-oriented convergence layers like TCP, a link object is
 * created whenever a new connection is made to a peer or when a
 * connection arrives from a peer. 
 */
class Link : public oasys::RefCountedObject,
             public oasys::Logger,
             public oasys::SerializableObject {
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
        UNAVAILABLE = 1,///< The link is closed and not able to be
                        ///  opened currently.

        AVAILABLE = 2,	///< The link is closed but is able to be
                        ///  opened, either because it is an on demand
                        ///  link, or because an opportunistic peer
                        ///  node is in close proximity but no
                        ///  convergence layer session has yet been
                        ///  opened.
        
        OPENING = 4,	///< A convergence layer session is in the
                        ///  process of being established.
        
        OPEN = 8,	///< A convergence layer session has been
                        ///  established, and the link has capacity
                        ///  for a bundle to be sent on it. This may
                        ///  be because no bundle is currently being
                        ///  sent, or because the convergence layer
                        ///  can handle multiple simultaneous bundle
                        ///  transmissions.
        
        BUSY = 16,	///< The link is busy, i.e. a bundle is
                        ///  currently being sent on it by the
                        ///  convergence layer and no more bundles may
                        ///  be delivered to the link.

        CLOSED = 32	///< Bogus state that's never actually used in
                        ///  the Link state_ variable, but is used for
                        ///  signalling the daemon thread with a
                        ///  LinkStateChangeRequest
        
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
        case CLOSED: 		return "CLOSED";
        }

        NOTREACHED;
    }
    
    /**
     * Static function to create appropriate link object from link type.
     */
    static LinkRef create_link(const std::string& name, link_type_t type,
                               ConvergenceLayer* cl, const char* nexthop,
                               int argc, const char* argv[],
                               const char** invalid_argp = NULL);

    /**
     * Constructor / Destructor
     */
    Link(const std::string& name, link_type_t type,
         ConvergenceLayer* cl, const char* nexthop);

    /**
     * Constructor for unserialization.
     */
    Link(const oasys::Builder& b);

    /**
     * Handle and mark deleted link.
     */
    virtual void delete_link();

    /**
     * Reconfigure the link parameters.
     */
    virtual bool reconfigure_link(int argc, const char* argv[]);

    virtual void reconfigure_link(AttributeVector& params);

    /**
     * Virtual from SerializableObject
     */
    void serialize(oasys::SerializeAction* action);

    /**
     * Hook for subclass to parse arguments.
     */
    virtual int parse_args(int argc, const char* argv[],
                           const char** invalidp = NULL);

    /**
     * Hook for subclass to post events to control the initial link
     * state, after all initialization is complete.
     */
    virtual void set_initial_state();

    /**
     * Return the type of the link.
     */
    link_type_t type() const { return static_cast<link_type_t>(type_); }

    /**
     * Return the string for of the link.
     */
    const char* type_str() const { return link_type_to_str(type()); }

    /**
     * Return whether or not the link is open.
     */
    bool isopen() const { return ( (state_ == OPEN) ||
                                   (state_ == BUSY) ); }

    /**
     * Return the availability state of the link.
     */
    bool isavailable() const { return (state_ != UNAVAILABLE); }

    /**
     * Return the busy state of the link.
     */
    bool isbusy() const { return (state_ == BUSY); }

    /**
     * Return whether the link is in the process of opening.
     */
    bool isopening() const { return (state_ == OPENING); }

    /**
     * Returns true if the link has been deleted; otherwise returns false.
     */
    bool isdeleted() const;

    /**
     * Return the actual state.
     */
    state_t state() const { return static_cast<state_t>(state_); }

    /**
     * Sets the state of the link. Performs various assertions to
     * ensure the state transitions are legal.
     *
     * This function should only ever be called by the main
     * BundleDaemon thread and helper classes. All other threads must
     * use a LinkStateChangeRequest event to cause changes in the link
     * state.
     *
     * The function isn't protected since some callers (i.e.
     * convergence layers) are not friend classes but some functions
     * are run by the BundleDaemon thread.
     */
    void set_state(state_t state);

    /**
     * Set/get the create_pending_ flag on the link.
     */
    void set_create_pending(bool create_pending = true)
             { create_pending_ = create_pending; }
    bool is_create_pending() const { return create_pending_; }

    /**
     * Set/get the usable_ flag on the link.
     */
    void set_usable(bool usable = true) { usable_ = usable; }
    bool is_usable() const { return usable_; }

    /**
     * Return the current contact information (if any).
     */
    const ContactRef& contact() const { return contact_; }

    /**
     * Set the contact information.
     */
    void set_contact(Contact* contact)
    {
        // XXX/demmer check this invariant
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
     * Accessor to local string
     */
    const char* local() const { return local_.c_str(); }

    /**
     * Mutator for local string
     */
    void set_local(const std::string& local) { local_.assign(local); }

    /**
     * Accessor to next hop string
     */
    const char* nexthop() const { return nexthop_.c_str(); }

    /**
     * Override for the next hop string.
     */
    void set_nexthop(const std::string& nexthop) { nexthop_.assign(nexthop); }

    /**
     * Accessor to the reliability bit.
     */
    bool is_reliable() const { return reliable_; }

    /**
     * Accessor to set the reliability bit when the link is created.
     */
    void set_reliable(bool r) { reliable_ = r; }

    /**
     * Accessor to set the remote endpoint id.
     */
    void set_remote_eid(const EndpointID& remote) {
        remote_eid_.assign(remote);
    }

    /**
     * Accessor to the remote endpoint id.
     */
    const EndpointID& remote_eid() { return remote_eid_; }

    /**
     * Accessor for the link's queue of bundles.
     */
    BundleList* queue() { return &queue_; }

    /**
     * Virtual from formatter
     */
    int format(char* buf, size_t sz) const;

    /**
     * Debugging printout.
     */
    void dump(oasys::StringBuffer* buf);

    /**************************************************************
     * Link Parameters
     */
    struct Params {
        /**
         * Default constructor.
         */
        Params();
        
        /**
         * MTU of the link, used to control proactive fragmentation.
         */
        u_int mtu_;
         
        /**
         * Minimum amount to wait between attempts to re-open the link
         * (in seconds).
         *
         * Default is set by the various Link types but can be overridden
         * by configuration parameters.
         */
        u_int min_retry_interval_;
    
        /**
         * Maximum amount to wait between attempts to re-open the link
         * (in seconds).
         *
         * Default is set by the various Link types but can be overridden
         * by configuration parameters.
         */
        u_int max_retry_interval_;

        /**
         * Seconds of idle time before the link is closed. Must be
         * zero for always on links (i.e. they are never closed).
         *
         * Default is 30 seconds for on demand links, zero for
         * opportunistic links.
         */
        u_int idle_close_time_;

        /**
         * Whether or not to send the previous hop header on this
         * link. Default is false.
         */
        bool prevhop_hdr_;

        /**
         * Abstract cost of the link, used by routing algorithms.
         * Default is 100.
         */
        u_int cost_;
    };

    /**
     * Seconds to wait between attempts to re-open an unavailable
     * link. Initially set to min_retry_interval_, then doubles up to
     * max_retry_interval_.
     */
    u_int retry_interval_;

    /**
     * Accessor for the parameter structure.
     */
    const Params& params() const { return params_; }
    Params& params() { return params_; }

    /*************************************************************
     * Link Statistics
     */
    struct Stats {
        /**
         * Number of times the link attempted to be opened.
         */
        u_int contact_attempts_;
        
        /**
         * Number of contacts ever successfully opened on the link
         * (equivalent to the number of times the link was open)
         */
        u_int contacts_;
        
        /**
         * Number of bundles transmitted
         */
        u_int bundles_transmitted_;

        /**
         * Total bytes transmitted (not counting convergence layer
         * overhead).
         */
        u_int bytes_transmitted_;

        /**
         * Number of bundles currently queued on the link.
         */
        u_int bundles_queued_;

        /**
         * Count of bytes currently queued on the link (not counting
         * convergence layer overhead).
         */
        u_int bytes_queued_;
        
        /**
         * Number of bundles currently in flight.
         */
        u_int bundles_inflight_;

        /**
         * Total bytes currently in flight (not counting convergence
         * layer overhead).
         */
        u_int bytes_inflight_;

        /**
         * Number of bundles with cancelled transmissions.
         */
        u_int bundles_cancelled_;

        /**
         * The availablity of the link, as measured over time by the
         * convergence layer.
         */
        u_int availability_;

        /**
         * The reliability of the link, as measured over time by the
         * convergence layer.  This is different from the is_reliable
         * setting, which indicates whether the convergence layer should
         * expect acks from the peer.
         */
        u_int reliability_;
    };

    /**
     * Accessor for the stats structure.
     */
    Stats* stats() { return &stats_; }

    /**
     * Reset the stats.
     */
    void reset_stats() const
    {
        memset(&stats_, 0, sizeof(stats_));
    }

    /**
     * Dump a printable version of the stats.
     */
    void dump_stats(oasys::StringBuffer* buf);

    /**
     * Accessor for the Link internal lock.
     */
    oasys::Lock* lock() { return &lock_; }
    
protected:
    friend class BundleActions;
    friend class BundleDaemon;
    friend class ContactManager;
    friend class ParamCommand;

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
    int type_;

    /// State of the link
    u_int32_t state_;

    /// Flag, that when set to true, indicates that the link has been deleted.
    bool deleted_;

    /// Flag, that when set to true, indicates that the creation of the
    /// link is pending; the convergence layer will post a creation event
    /// when the creation is complete. While creation is pending, the
    /// link cannot be opened nor can bundles be queued for it.
    bool create_pending_;

    /// Flag, that when set to true, indicates that the link is allowed
    /// to be used to transmit bundles.
    bool usable_;

    /// Local address (optional)
    std::string local_;
    
    /// Next hop address
    std::string nexthop_;
    
    /// Internal name of the link 
    std::string name_;

    /// Whether or not this link is reliable
    bool reliable_;

    /// Parameters of the link
    Params params_;

    /// Default parameters of the link
    static Params default_params_;

    /// Queue of bundles currently active or pending transmission on the Link
    BundleList queue_;

    /// Stats for the link
    mutable Stats stats_;

    /// Current contact. contact_ != null iff link is open
    ContactRef contact_;

    /// Pointer to convergence layer
    ConvergenceLayer* clayer_;

    /// Convergence layer specific info, if needed
    CLInfo* cl_info_;

    /// Remote's endpoint ID (eg, dtn://hostname.dtn)
    EndpointID remote_eid_;

    /// Lock to protect internal data structures and state.
    oasys::SpinLock lock_;
    
    /// Destructor -- protected since links shouldn't be deleted
    virtual ~Link();
};

} // namespace dtn

#endif /* _LINK_H_ */

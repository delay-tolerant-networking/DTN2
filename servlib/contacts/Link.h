#ifndef _LINK_H_
#define _LINK_H_

class LinkInfo;
class ConvergenceLayer;
class Contact;
class Peer;

#include "Peer.h"
#include "BundleConsumer.h"
#include "BundleTuple.h"
#include "BundleList.h"

#include <set>

/**
 * Valid types for a link.
 */
typedef enum
{
    LINK_INVALID = -1,
    
    /**
     * The link is expected to be either ALWAYS available, or can
     * be made available easily. Examples include DSL (always), and
     * dialup (easily available).
     */
    ONDEMAND = 1,
    
    /**
     * The link is only available at pre-determined times.
     */
    SCHEDULED = 2,

    /**
     * The link may or may not be available, based on
     * uncontrollable factors. Examples include a wireless link whose
     * connectivity depends on the relative locations of the two
     * nodes.
     */
    OPPORTUNISTIC = 3
}
link_type_t;

/**
 * Link type string conversion.
 */
inline const char*
link_type_to_str(link_type_t type)
{
    switch(type) {
    case ONDEMAND: 	return "ONDEMAND";
    case SCHEDULED: 	return "SCHEDULED";
    case OPPORTUNISTIC: return "OPPORTUNISTIC";
    default: 		PANIC("bogus link_type_t");
    }
}

inline link_type_t
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
 * Abstraction for a DTN link. A DTN link has a destination peer
 * associated with it. The peer represents the DTN node to which the
 * link leads to. A link can be in two states: open or closed.
 * An open link has a uniquie non-null contact associated with it.
 * A link may also have a queue for storing bundles if no contact
 * is currently active on it. Every link has a unique name asssociated
 * with it which is used to identify it. The name is configured 
 * explicitly when link is created
 *
 * Creating new links:
 * Links are created explicitly as configuration. Syntax is:
 * route link_create <name> <dest_tuple> <type> <conv_layer> <params>
 * See RouteCommand.h
 * XXX/Sushant Fix order of arguments ??
 *
 *
 *
 * Opening a link means => Creating a new contact on it.
 * It is an error to open a link which is already open.
 * One can check status, if the link is already open or not.
 * Links are either opened automatically (for ondemand links)
 * or explicitly based on schedule by the contact-manager.
 *
 * Closing a link means => Deleting or getting rid of existing contact
 * on it. Free the state stored for that contact. Its an error
 * to close a link which is already closed.
 *
 * Opening/Closing of link can be called only
 * by the -router/contact-manager-.
 * The link  close process can be started  by the convergence layer
 * on detecting a failure. It does it by generating CONTACT_BROKEN event which
 * is passed to the bundle router which than closes the link by calling
 * link->close()
 *
 *
 * Links are of three types as discussed in the DTN architecture
 * ONDEMAND, SCHEDULED, OPPORTUNISTIC.
 * The key differences from an implementation perspectives are
 * "who" and "when" opens and closes the link.
 *
 * Only ONDEMAND links can not be explicitly opened.
 * Only when one tries to send bundle over a link itis opened.
 * ONDEMAND links do not have a queue associated with link.
 * They only have a queue associated with the contact.
 * An ONDEMAND link can be closed either because of convergence
 * layer failure or because the link has been idle for too long.
 *
 *
 * OPPORTUNISTIC links have a queue associated with them.
 * The queue contains bundles which are waiting for a contact to
 * happen. When a contact happens on that link it is the responsibility
 * of the routing monster to transfer message from one queue to
 * another. Similarly when a contact is broken  router performs
 * the transferring of messages from contact'a queue to link's queue.
 * The key is how is the "contact created" at the first place.
 * We believe the contact manager will take care of this.
 * ONDEMAND links may also store history or any other aggregrate
 * statistics in link info if they need.
 *
 * We enforce the invariant that
 * if contact != null then size(link_queue) == 0.
 * i.e link queue can not have any messages on it if an active
 * contact exists. This is handle by the enqueue function for 
 * Link class.
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
 * An interesting question is to defined the order in which the messages
 * from the link queue and the contact queue are merged. This is because
 * router may already have scheduled some messages on the queue of
 * current contact when it was a future contact.
 *
 *
*/
class Link : public Formatter, public BundleConsumer {
public:

    typedef std::set<Link*> LinkSet;    

    /**
     * Static function to create appropriate link object from link type
     */
    static Link* create_link(std::string name, link_type_t type,
                             const char* conv_layer,
                             const BundleTuple& tuple);
    /**
     * Constructor / Destructor
     */
    Link(std::string name, link_type_t type, const char* conv_layer,
         const BundleTuple& tuple);
    virtual ~Link();
    

    /**
     * Return the type of the link.
     */
    link_type_t type_link() { return type_; }

    /**
     * Return the string for of the link.
     */
    const char* type_str() { return link_type_to_str(type_); }

    /**
     * Accessor to peer.
     */
    Peer* peer() { return peer_; }

    /**
     * Return the link's current contact
     */
    Contact* contact() { return contact_; }
    
    /**
     * Return the state of the link.
     */
    bool isopen() { return contact_ != NULL; }

    
    /**
     * Add the bundle to the queue.
     */
    virtual void enqueue_bundle(Bundle* bundle, const BundleMapping* mapping);
    
    /**
     * Attempt to remove the given bundle from the queue.
     *
     * @return true if the bundle was dequeued, false if not. If
     * mappingp is non-null, return the old mapping as well.
     */
    // virtual bool dequeue_bundle(Bundle* bundle, BundleMapping** mappingp);
    
    /**
     * Store the implementation specific part of the link.
     */
    void set_link_info(LinkInfo* link_info)
    {
        ASSERT(link_info_ == NULL || link_info == NULL);
        link_info_ = link_info;
    }

    /**
     * Accessor to the link info.
     */
    LinkInfo* link_info() { return link_info_; }
    
    /**
     * Accessor to this contact's convergence layer.
     */
    ConvergenceLayer* clayer() { return clayer_; }

    /**
     * Accessor to this links name
     */
    const char* name() { return name_.c_str(); }

    /**
     * Accessor to tuple
     */
    BundleTuple tuple() { return tuple_; }

    /**
     * Virtual from formatter
     */
    int format(char* buf, size_t sz);

    /**
     * Virtual from bundle consumer
     */
    const char* type() { return "Link" ;}
    
protected:

    friend class ContactManager;
    friend class BundleRouter;
    
    /**
     * Open/Close link
     * It is protected to not prevent random system
     * components call open/close
     */
    virtual void open();
    virtual void close();

    // Type of the link
    link_type_t type_;

    // Associated destination bundle-tuple
     BundleTuple tuple_;
    
    // Name of the link to identify across daemon
    std::string name_;

    // Peer is initialized in constructor using bundleTuple 
    Peer* peer_;

    // Current contact. contact_ != null iff link is open
    Contact* contact_;

    // Convergence layer specific info, if needed
    LinkInfo* link_info_;

    // Pointer to convergence layer
    ConvergenceLayer* clayer_;

};


/**
 * Abstract base class for convergence layer specific portions of a
 * link.
 */
class LinkInfo {
public:
    virtual ~LinkInfo() {}
};

#endif /* _LINK_H_ */

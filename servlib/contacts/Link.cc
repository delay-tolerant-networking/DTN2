#include "Link.h"
#include "conv_layers/ConvergenceLayer.h"
#include "ContactManager.h"
#include "OndemandLink.h"
#include "ScheduledLink.h"
#include "OpportunisticLink.h"
#include "BundleForwarder.h"
#include "BundleEvent.h"

/**
 * Static constructor to create different type of links
 */
Link*
Link::create_link(std::string name, link_type_t type,
                             const char* conv_layer,
                        const BundleTuple& tuple)
{
    switch(type) {
    case ONDEMAND: 	return new OndemandLink(name,conv_layer,tuple);
    case SCHEDULED: 	return new ScheduledLink(name,conv_layer,tuple);
    case OPPORTUNISTIC: return new OpportunisticLink(name,conv_layer,tuple);
    default: 		PANIC("bogus link_type_t");
    }
}

/**
 * Constructor
 */
Link::Link(std::string name, link_type_t type, const char* conv_layer,
           const BundleTuple& tuple)
    :  BundleConsumer(&tuple_, false),
    type_(type),   tuple_(tuple),  name_(name)
{

    logpathf("/link/%s",name_.c_str());

    // Find convergence layer
    clayer_ = ConvergenceLayer::find_clayer_proto(conv_layer);
    if (!clayer_) {
        PANIC("can't find convergence layer for %s", tuple.admin().c_str());
        // XXX/demmer need better error handling
    }
    
    // Contact manager will create a peer if peer does not exist
    if (!tuple.valid()) {
        PANIC("malformed peer %s",tuple.c_str());
        // XXX/demmer need better error handling
    }
    peer_ = ContactManager::instance()->find_peer(tuple);
    if (peer_ == NULL) {
        peer_ = new Peer(tuple);
        ContactManager::instance()->add_peer(peer_);
    }
    peer_->add_link(this);
     
    // By default link does not have an associated contact
    contact_ = NULL ;

    /*
     * Note that bundle list is not initialized, for ondemand links it
     * must be null
     */
    bundle_list_ = NULL;
    link_info_ = NULL;
    
    log_info("new link *%p", this);

    // Add the link to contact manager
    ContactManager::instance()->add_link(this);
    
    // Post a link created event
    BundleForwarder::post(new LinkCreatedEvent(this));
}
    

Link::~Link()
{
    if (bundle_list_ != NULL) {
        ASSERT(bundle_list_->size() == 0);
        delete bundle_list_;
    }
// XXX Sushant, should one  post a link delete event
}

/**
 * Open a channel to the link for bundle transmission.
 * Relevant only if Link is ONDEMAND
 * Move this to subclasses ? XXX/Sushant
 *
 */
void
Link::open()
{
    log_debug("Link::open");
    if (!isopen()) {
        contact_ = new Contact(this);
    } else {
        log_warn("Trying to open an already open link %s",name());
    }
}
    
/**
 * Close the currently active contact on the link.
 * This is typically called in the following scenario's
 *
 * By BundleRouter:  when it receives contact_broken event
 *                :  when it thinks it has sent all messages for
 *                   an on demand link and it no longer needs it
 * By ContactManager: when a scheduled link uptime finishes
 *                  : when an opportunistic link has gone away
 *
 * In generaly any relevant entity in the system can try to close
 * an active contact based upon whatever logic is desired.
 * Currently we force that only BundleRuter/ContactManager can make
 * these calls
 *
 * An important requirement is to ensure that the bundle_list_
 * on the contact does not have any pending messages
 *
 */
void
Link::close()
{
    log_debug("Link::close");

    // Ensure that link is open
    ASSERT(contact_ != NULL);

    // Assert contact bundle list is empty
    ASSERT(contact_->bundle_list()->size()==0);
    
    //Close the contact
    clayer()->close_contact(contact_);
    ASSERT(contact_->contact_info() == NULL) ; 

    // Actually nullify the contact.
    // This is important because link == Open iff contact_ == NULL
    contact_ = NULL;
}

/**
 * Formatting
 */
int
Link::format(char* buf, size_t sz)
{
    return snprintf(buf, sz, "%s: %s",name(),link_type_to_str(type_));
}

/**
 * Overrided default queuing behavior to handle ONDEMAND links
 * For other types of links invoke default behavior
 */
void
Link::enqueue_bundle(Bundle* bundle, const BundleMapping* mapping)
{
    // XXX/sushant move to OndemandLink
    // For ondemand link open the link if it is already not open
    if (type_ == ONDEMAND) {
        if (!isopen()) {
            open();
        }
    }

    /*
     * If the link is open, messages are always queued on the contact
     * queue, if not, put them on the link queue.
     */
    if (isopen()) {
        contact_->enqueue_bundle(bundle,mapping);
    } else {
        BundleConsumer::enqueue_bundle(bundle,mapping);
    }
    
}

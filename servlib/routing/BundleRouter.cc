
#include "BundleRouter.h"
#include "RouteTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleList.h"
#include "bundling/FragmentManager.h"
#include "reg/Registration.h"
#include <stdlib.h>

#include "StaticBundleRouter.h"

std::string BundleRouter::type_;
size_t BundleRouter::proactive_frag_threshold_;

/**
 * Factory method to create the correct subclass of BundleRouter
 * for the registered algorithm type.
 */
BundleRouter*
BundleRouter::create_router(const char* type)
{
    if (!strcmp(type, "static")) {
        return new StaticBundleRouter();
    } else {
        PANIC("unknown type %s for router", type);
    }
}

/**
 * Constructor.
 */
BundleRouter::BundleRouter()
    : Logger("/route")
{
    route_table_ = new RouteTable();
    pending_bundles_ = new BundleList("pending_bundles");
    custody_bundles_ = new BundleList("custody_bundles");
}

/**
 * Destructor
 */
BundleRouter::~BundleRouter()
{
    NOTIMPLEMENTED;
}

/**
 * The monster routing decision function that is called in
 * response to all received events. To cause bundles to be
 * forwarded, this function populates the given action list with
 * the forwarding decisions. The run() method takes the decisions
 * from the active router and passes them to the BundleForwarder.
 *
 * Note that the function is virtual so more complex derived
 * classes can override or augment the default behavior.
 */
void
BundleRouter::handle_event(BundleEvent* e,
                           BundleActionList* actions)
{
    switch(e->type_) {

    case BUNDLE_RECEIVED:
        handle_bundle_received((BundleReceivedEvent*)e, actions);
        break;

    case BUNDLE_TRANSMITTED:
        handle_bundle_transmitted((BundleTransmittedEvent*)e, actions);
        break;
        
    case REGISTRATION_ADDED:
        handle_registration_added((RegistrationAddedEvent*)e, actions);
        break;

    case ROUTE_ADD:
        handle_route_add((RouteAddEvent*)e, actions);
        break;

    case CONTACT_AVAILABLE:
	log_info("Ignoring contact available event...no action taken");
        break;

    case CONTACT_BROKEN:
	log_info("Ignoring contact broken event...no action taken");
        break;

    default:
        PANIC("unimplemented event type %d", e->type_);
    }

    delete e;
}

/**
 * Default event handler for new bundle arrivals.
 *
 * Queue the bundle on the pending delivery list, and then
 * searches through the route table to find any matching next
 * contacts, filling in the action list with forwarding decisions.
 */
void
BundleRouter::handle_bundle_received(BundleReceivedEvent* event,
                                     BundleActionList* actions)
{
    Bundle* bundle = event->bundleref_.bundle();
    
    log_debug("BUNDLE_RECEIVED bundle id %d", bundle->bundleid_);

    // XXX/demmer test for fragmentation code
    if (bundle->payload_.length() > proactive_frag_threshold_) {
        Bundle* fragment;
        
        int todo = bundle->payload_.length();
        int offset = 0;
        int fraglen;
        while (todo > 0) {
            fraglen = random() % proactive_frag_threshold_;
            if (fraglen < 100) fraglen = 100;
            if ((fraglen + offset) > (int)bundle->payload_.length()) {
                fraglen = bundle->payload_.length() - offset;
            }

            fragment = FragmentManager::instance()->fragment(bundle, offset, fraglen);
            ASSERT(fragment);

            pending_bundles_->push_back(fragment);
            actions->push_back(new BundleAction(STORE_ADD, fragment));
            fwd_to_matching(fragment, actions);
            
            offset += fraglen;
            todo -= fraglen;
        }
        return;
    }

    pending_bundles_->push_back(bundle);
    actions->push_back(new BundleAction(STORE_ADD, bundle));
    fwd_to_matching(bundle, actions);
}

void
BundleRouter::handle_bundle_transmitted(BundleTransmittedEvent* event,
                                        BundleActionList* actions)
{
    /**
     * The bundle was delivered to either a next-hop contact or a
     * registration.
     */
    Bundle* bundle = event->bundleref_.bundle();

    log_debug("BUNDLE_TRANSMITTED bundle id %d (%d bytes) %s -> %s",
              bundle->bundleid_, event->bytes_sent_,
              event->acked_ ? "ACKED" : "UNACKED",
              event->consumer_->dest_tuple()->c_str());
        
    /*
     * Check for reactive fragmentation, potentially splitting off
     * the unsent portion, if necessary.
     */
    if (event->bytes_sent_ != bundle->payload_.length()) {
        PANIC("reactive fragmentation not implemented");
    }

    /*
     * If the whole bundle was sent and this is the last destination
     * that needs a copy of the bundle, we can safely remove it from
     * the pending list.
     */
    if (bundle->num_containers() == 1) {
        log_debug("last consumer, removing bundle from pending list");

        // XXX/demmer this should go through the bundle's backpointer
        // iterator rather than traversing the whole pending bundles
        // list
        bool removed = pending_bundles_->remove(bundle);
        ASSERT(removed);
        
        actions->push_back(new BundleAction(STORE_DEL, bundle));
    }
}

/**
 * Default event handler when a new application registration
 * arrives.
 *
 * Adds an entry to the route table for the new registration, then
 * walks the pending list to see if any bundles match the
 * registration.
 */
void
BundleRouter::handle_registration_added(RegistrationAddedEvent* event,
                                        BundleActionList* actions)
{
    Registration* registration = event->registration_;
    log_debug("new registration for %s", registration->endpoint().c_str());

    RouteEntry* entry = new RouteEntry(registration->endpoint(),
                                       registration, FORWARD_COPY);
    route_table_->add_entry(entry);
    new_next_hop(registration->endpoint(), registration, actions);
}

/**
 * Default event handler when a new route is added by the command
 * or management interface.
 *
 * Adds an entry to the route table, then walks the pending list
 * to see if any bundles match the new route.
 */
void
BundleRouter::handle_route_add(RouteAddEvent* event,
                               BundleActionList* actions)
{
    RouteEntry* entry = event->entry_;
    route_table_->add_entry(entry);
    new_next_hop(entry->pattern_, entry->next_hop_, actions);
}

/**
 * Loop through the routing table, adding an entry for each match
 * to the action list.
 */
void
BundleRouter::fwd_to_matching(Bundle* bundle, BundleActionList* actions)
{
    RouteEntry* entry;
    RouteEntrySet matches;
    RouteEntrySet::iterator iter;

    size_t count = route_table_->get_matching(bundle->dest_, &matches);
    
    log_debug("fwd_to_matching %s: %d matches", bundle->dest_.c_str(), count);

    for (iter = matches.begin(); iter != matches.end(); ++iter) {
        entry = *iter;
        actions->push_back(
            new BundleForwardAction(entry->action_, bundle, entry->next_hop_));
    }
}

/**
 * Called whenever a new consumer (i.e. registration or contact)
 * arrives. This should walk the list of unassigned bundles (or
 * maybe all bundles???) to see if the new consumer matches.
 */
void
BundleRouter::new_next_hop(const BundleTuplePattern& dest,
                           BundleConsumer* next_hop,
                           BundleActionList* actions)
{
    log_debug("XXX/demmer implement new_next_hop");
}

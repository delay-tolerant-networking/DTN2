
#include "BundleRouter.h"
#include "RouteTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleList.h"
#include "bundling/Contact.h"
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
        handle_contact_available((ContactAvailableEvent*)e, actions);
        break;

    case CONTACT_BROKEN:
        handle_contact_broken((ContactBrokenEvent*)e, actions);
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

    /*
     * Check if the bundle isn't complete. If so, do reactive
     * fragmentation.
     *
     * XXX/demmer this should maybe be changed to do the reactive
     * fragmentation in the forwarder?
     */
    if (event->bytes_received_ != bundle->payload_.length()) {
        log_debug("partial bundle, making fragment of %d bytes",
                  event->bytes_received_);
        FragmentManager::instance()->
            convert_to_fragment(bundle, event->bytes_received_);
    }

    pending_bundles_->push_back(bundle);
    actions->push_back(new BundleAction(STORE_ADD, bundle));
    fwd_to_matching(bundle, actions, true);
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
     * If the whole bundle was sent and this is the last destination
     * that needs a copy of the bundle, we can safely remove it from
     * the pending list.
     */
    if (bundle->num_containers() == 1) {
        log_debug("last consumer, removing bundle from pending list");
        log_debug("XXX/demmer this is bogus");

        // XXX/demmer this should go through the bundle's backpointer
        // iterator rather than traversing the whole pending bundles
        // list
        bool removed = pending_bundles_->remove(bundle);
        ASSERT(removed);
        
        actions->push_back(new BundleAction(STORE_DEL, bundle));
    }

    /*
     * Check for reactive fragmentation, potentially splitting off the
     * unsent portion as a new bundle, if necessary.
     */
    size_t payload_len = bundle->payload_.length();

    if (event->bytes_sent_ != payload_len) {
        size_t frag_off = event->bytes_sent_;
        size_t frag_len = payload_len - event->bytes_sent_;

        log_debug("incomplete transmission: creating reactive fragment "
                  "(offset %d len %d/%d)", frag_off, frag_len, payload_len);

        Bundle* tail = FragmentManager::instance()->
                       create_fragment(bundle, frag_off, frag_len);
        
        // treat the new fragment as if it just arrived.
        pending_bundles_->push_back(tail);
        actions->push_back(new BundleAction(STORE_ADD, tail));
        fwd_to_matching(tail, actions, false);
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
                                       registration, FORWARD_REASSEMBLE);
    entry->local_ = true;
    route_table_->add_entry(entry);
    new_next_hop(registration->endpoint(), registration, actions);
}

/**
 * Default event handler when a new contact is available.
 */
void
BundleRouter::handle_contact_available(ContactAvailableEvent* event,
                                       BundleActionList* actions)
{
    log_info("CONTACT_AVAILABLE *%p", event->contact_);
}

/**
 * Default event handler when a contact is broken
 */
void
BundleRouter::handle_contact_broken(ContactBrokenEvent* event,
                                    BundleActionList* actions)
{
    Contact* contact = event->contact_;
    log_info("CONTACT_BROKEN *%p: re-routing queued bundles", contact);
    
    Bundle* bundle;
    BundleList::iterator iter;

    BundleList temp("temp");
    contact->bundle_list()->move_contents(&temp);
    
    ScopeLock lock(temp.lock());
    
    for (iter = temp.begin(); iter != temp.end(); ++iter) {
        bundle = *iter;
        fwd_to_matching(bundle, actions, false);
    }

    contact->close();
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
BundleRouter::fwd_to_matching(Bundle* bundle, BundleActionList* actions,
                              bool include_local)
{
    RouteEntry* entry;
    RouteEntrySet matches;
    RouteEntrySet::iterator iter;

    size_t count = route_table_->get_matching(bundle->dest_, &matches);
    
    log_debug("fwd_to_matching %s: %d matches", bundle->dest_.c_str(), count);

    for (iter = matches.begin(); iter != matches.end(); ++iter) {
        entry = *iter;
        if ((include_local == false) && entry->local_)
            continue;
        
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

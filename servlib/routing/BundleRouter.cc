
#include "BundleRouter.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleList.h"
#include "reg/Registration.h"

BundleRouterList BundleRouter::routers_;

/**
 * Add a new router algorithm to the list.
 */
void
BundleRouter::register_router(BundleRouter* router)
{
    __log_debug("/route", "registering router %p", router);
    routers_.push_back(router);
}

/**
 * Dispatch the event to each registered router.
 */
void
BundleRouter::dispatch(BundleEvent* event)
{
    // XXX/demmer need to copy the event and push it on all queues
    if (routers_.size() != 1) {
        PANIC("multiple router algorithms not yet implemented");
    }

    __log_debug("/route", "queuing event with type %d", event->type_);
    routers_[0]->eventq_.push(event);
}

/**
 * Constructor.
 */
BundleRouter::BundleRouter()
    : Logger("/route")
{
    pending_bundles_ = new BundleList();
    custody_bundles_ = new BundleList();
}

/**
 * Add a route entry.
 */
bool
BundleRouter::add_route(const BundleTuplePattern& dest,
                        BundleConsumer* next_hop,
                        bundle_action_t action,
                        int argc, const char** argv)
{
    // XXX/demmer check for duplicates?
    RouteEntry* entry = new RouteEntry(dest, next_hop, action);
    log_debug("add_route %s -> %s (%s)", dest.c_str(),
              next_hop->dest_tuple()->c_str(), bundle_action_toa(action));
    route_table_.insert(entry);
    return true;
}

/**
 * Remove a route entry.
 */
bool
BundleRouter::del_route(const BundleTuplePattern& dest,
                        BundleConsumer* next_hop)
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

    case BUNDLE_RECEIVED: {
        /*
         * Queue the bundle on the pending delivery list, and search
         * through the route table to find any matching next contacts,
         * filling in the action list.
         */
        BundleReceivedEvent* event = (BundleReceivedEvent*)e;
        Bundle* bundle = event->bundleref_.bundle();

        log_debug("BUNDLE_RECEIVED bundle id %d", bundle->bundleid_);

        pending_bundles_->push_back(bundle);
        get_matching(bundle, actions);
        
        break;
    }

    case BUNDLE_TRANSMITTED: {
        /**
         * The bundle was sent to either a next-hop contact or a
         * registration.
         */
        BundleTransmittedEvent* event = (BundleTransmittedEvent*)e;
        Bundle* bundle = event->bundleref_.bundle();

        log_debug("BUNDLE_TRANSMITTED bundle id %d (%d bytes) %s",
                  bundle->bundleid_, event->bytes_sent_,
                  event->acked_ ? "ACKED" : "UNACKED");
        
        /*
         * Check for reactive fragmentation, potentially splitting off
         * the unsent portion, if necessary.
         */
        if (event->bytes_sent_ != bundle->payload_.length()) {
            PANIC("reactive fragmentation not implemented");
        }

        /*
         * If the whole bundle was sent and this is the last
         * destination that needs a copy of the bundle, we can safely
         * remove it from the pending list.
         *
         * Note that removing the bundle from the pending list may
         * remove the last reference on the bundle, which may delete
         * the bundle.
         */
        if (bundle->num_containers() == 1) {
            log_debug("last consumer, removing bundle from pending list");
            bool removed = pending_bundles_->remove(bundle);
            ASSERT(removed);
        }
        
        break;
    };

    case REGISTRATION_ADDED: {
        /*
         * Add an entry to the routing table and see if any queued
         * bundles should be forwarded to it.
         */
        RegistrationAddedEvent* event = (RegistrationAddedEvent*)e;
        Registration* registration = event->registration_;
        log_debug("new registration for %s", registration->endpoint().c_str());
        add_route(registration->endpoint(), registration, FORWARD_COPY);
        handle_next_hop(registration->endpoint(), registration, actions);
        break;
    }

    default:
        PANIC("unimplemented event type %d", e->type_);
    }

    log_debug("dispatch complete, cleaning up event");
    delete e;
}

/**
 * Loop through the routing table, adding an entry for each match
 * to the action list.
 */
void
BundleRouter::get_matching(Bundle* bundle, BundleActionList* actions)
{
    RouteTable::iterator iter;
    RouteEntry* entry;
    int count;

    log_debug("get_matching %s", bundle->dest_.c_str());
    for (iter = route_table_.begin(); iter != route_table_.end(); ++iter) {
        entry = *iter;
        if (entry->pattern_.match(bundle->dest_)) {
            ++count;
            log_debug("match entry %s -> %s (%s)",
                      entry->pattern_.c_str(),
                      entry->next_hop_->dest_tuple()->c_str(),
                      bundle_action_toa(entry->action_));
            actions->push_back(new BundleAction(entry->action_, bundle, entry->next_hop_));
        }
    }
    log_debug("get_matching done, %d matches", count);
}

/**
 * Called whenever a new consumer (i.e. registration or contact)
 * arrives. This should walk the list of unassigned bundles (or
 * maybe all bundles???) to see if the new consumer matches.
 */
void
BundleRouter::handle_next_hop(const BundleTuplePattern& dest,
                              BundleConsumer* next_hop,
                              BundleActionList* actions)
{
    log_debug("XXX/demmer implement me");
}


/**
 * The main run loop.
 */
void
BundleRouter::run()
{
    BundleEvent* event;
    BundleForwarder* forwarder = BundleForwarder::instance();
    BundleActionList actions;
    
    while (1) {
        // grab an event off the queue, blocking until we get one
        event = eventq_.pop();
        ASSERT(event);

        // dispatch to fill in the actions list
        actions.clear();
        handle_event(event, &actions);

        // if we're the active routing algorithm, then send our
        // actions to the forwarder to "make it so"
        if (active() && actions.size() != 0) {
            forwarder->process(&actions);
        }
    }
}

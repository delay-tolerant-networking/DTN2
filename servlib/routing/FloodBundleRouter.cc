
#include "BundleRouter.h"
#include "RouteTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleList.h"
#include "bundling/Contact.h"
#include "bundling/FragmentManager.h"
#include "reg/Registration.h"
#include <stdlib.h>

#include "FloodBundleRouter.h"

const BundleTuplePattern& ALL_TUPLES("bundles://*/*");

/**
 * Constructor.
 */
FloodBundleRouter::FloodBundleRouter()
{
}

/**
 * Destructor
 */
FloodBundleRouter::~FloodBundleRouter()
{
    NOTIMPLEMENTED;
}

/**
 * Default event handler for new bundle arrivals.
 *
 * Queue the bundle on the pending delivery list, and then
 * searches through the route table to find any matching next
 * contacts, filling in the action list with forwarding decisions.
 */
void
FloodBundleRouter::handle_bundle_received(BundleReceivedEvent* event,
                                     BundleActionList* actions)
{
    Bundle* bundle = event->bundleref_.bundle();

    // bundle is always pending until expired or acked
    // note: ack is not implemented
    bundle->add_pending();

    BundleRouter::handle_bundle_received(event, actions);
}

/**
 * Default event handler when a new contact is available.
 */
void
FloodBundleRouter::handle_contact_available(ContactAvailableEvent* event,
                                       BundleActionList* actions)
{
    Contact * contact = event->contact_;
    log_info("CONTACT_AVAILABLE *%p", event->contact_);

    RouteEntry* entry = new RouteEntry(ALL_TUPLES, contact,
                                       FORWARD_COPY);
    entry->local_ = true;
    route_table_->add_entry(entry);
    new_next_hop(ALL_TUPLES, contact, actions);
}

/**
 * Default event handler when a contact is broken
 */
void
FloodBundleRouter::handle_contact_broken(ContactBrokenEvent* event,
                                    BundleActionList* actions)
{
    Contact* contact = event->contact_;
    log_info("CONTACT_BROKEN *%p: removing queued bundles", contact);
    
    //XXX not implemented yet - neeed to do
    //route_table_.del_entry(ALL_TUPLES, contact);
    // empty contact list
    // for flood, no need to maintain bundle list
    contact->bundle_list()->clear();

    
    contact->close();
}

/**
 * Called whenever a new consumer (i.e. registration or contact)
 * arrives. This should walk the list of unassigned bundles (or
 * maybe all bundles???) to see if the new consumer matches.
 */
void
FloodBundleRouter::new_next_hop(const BundleTuplePattern& dest,
                           BundleConsumer* next_hop,
                           BundleActionList* actions)
{
    log_debug("XXX/demmer implement new_next_hop");

    Bundle* bundle;
    BundleList::iterator iter;

    ScopeLock lock(pending_bundles_->lock());
    
    // upon arrival of new contact, send all pending bundles over contact
    for (iter = pending_bundles_->begin(); 
         iter != pending_bundles_->end(); ++iter) {
        bundle = *iter;
        actions->push_back(
            new BundleForwardAction(FORWARD_COPY, bundle, next_hop));
    }
}


#include "BundleRouter.h"
#include "RouteTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleList.h"
#include "bundling/Contact.h"
#include "bundling/FragmentManager.h"
#include "reg/Registration.h"
#include <stdlib.h>

#include "FloodBundleRouter.h"
//#include "debug/Debug.h"
#include <stdlib.h>

/**
 * Constructor.
 */
FloodBundleRouter::FloodBundleRouter()
    : all_tuples_("bundles://*/*:*")
{
    log_info("FLOOD_ROUTER");

//    router_stats_= new RouterStatistics(this);
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
    log_info("FLOOD: bundle_rcv bundle id %d", bundle->bundleid_);
    
    /*
     * Check if the bundle isn't complete. If so, do reactive
     * fragmentation.
     *
     * XXX/demmer this should maybe be changed to do the reactive
     * fragmentation in the forwarder?
     */
    if (event->bytes_received_ != bundle->payload_.length()) {
        log_info("XXX: PARTIAL bundle:%d, making fragment of %d bytes",
                  bundle->bundleid_,event->bytes_received_);
        FragmentManager::instance()->
            convert_to_fragment(bundle, event->bytes_received_);
    }
        

    //statistics
    //bundle has been accepted
    //router_stats_->add_bundle_hop(bundle);

    

    Bundle* iter_bundle;
    BundleList::iterator iter;

    ScopeLock lock(pending_bundles_->lock());
    
    // bundle is always pending until expired or acked
    // note: ack is not implemented
    bundle->add_pending();
    
    //check if we already have the bundle with us ... 
    //then dont enqueue it
    // upon arrival of new contact, send all pending bundles over contact
    for (iter = pending_bundles_->begin(); 
         iter != pending_bundles_->end(); ++iter) {
        iter_bundle = *iter;
        log_info("\tpending_bundle:%d size:%d pending:%d",
                  iter_bundle->bundleid_,iter_bundle->payload_.length(),
                  iter_bundle->pendingtx());
        if(iter_bundle->bundleid_ == bundle->bundleid_) {
            //delete the bundle
            return;
        }
    }
    
    pending_bundles_->push_back(bundle, NULL);
    if (event->source_ != EVENTSRC_PEER)
        actions->push_back(new BundleAction(STORE_ADD, bundle));
    
    //here we do not need to handle the new bundle immediately
    //just put it in the pending_bundles_ queue, and it
    //needs to be used only when a new contact comes up
    //**might do something different if the bundle is from
    //  the local node
    //BundleRouter::handle_bundle_received(event, actions);

    fwd_to_matching(bundle,actions,true);
}

void
FloodBundleRouter::handle_bundle_transmitted(BundleTransmittedEvent* event,
                                        BundleActionList* actions)
{
    Bundle* bundle = event->bundleref_.bundle();
    
    bundle->add_pending();
    
    //only call the fragmentation routine if we send nonzero bytes
    if(event->bytes_sent_ > 0) {
        BundleRouter::handle_bundle_transmitted(event,actions);         
    } else {
        log_info("FLOOD: transmitted ZERO bytes:%d",bundle->bundleid_);
    }
    
    //now we want to ask the contact to send the other queued
    //bundles it has

    PANIC("XXX/demmer need to kick the contact to tell it to send bundleS");
    //Contact * contact = (Contact *)event->consumer_;
    //contact->enqueue_bundle(NULL, FORWARD_UNIQUE);
    
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
FloodBundleRouter::handle_registration_added(RegistrationAddedEvent* event,
                                        BundleActionList* actions)
{
    Registration* registration = event->registration_;
    log_info("FLOOD: new registration for %s", registration->endpoint().c_str());

    RouteEntry* entry = new RouteEntry(registration->endpoint(),
                                       registration, FORWARD_REASSEMBLE);
    route_table_->add_entry(entry);
 
    //BundleRouter::handle_registration_added(event,actions);
}

/**
 * Default event handler when a new link is created.
 */
void
FloodBundleRouter::handle_link_created(LinkCreatedEvent* event,
                                       BundleActionList* actions)
{
    
    Link* link = event->link_;
    ASSERT(link != NULL);
    log_info("FLOOD: LINK_CREATED *%p", event->link_);

    RouteEntry* entry = new RouteEntry(all_tuples_, link, FORWARD_COPY);
    route_table_->add_entry(entry);

    //first clear the list with the contact
//    contact->bundle_list()->clear();

    //copy the pending_bundles_ list into a new exchange list
    //exchange_list_ = pending_bundles_->copy();
    //
    new_next_hop(all_tuples_, link, actions);
}

/**
 * Default event handler when a contact is down
 */
void
FloodBundleRouter::handle_contact_down(ContactDownEvent* event,
                                    BundleActionList* actions)
{
    Contact* contact = event->contact_;
    log_info("FLOOD: CONTACT_DOWN *%p: removing queued bundles", contact);
    
    //XXX not implemented yet - neeed to do
    route_table_->del_entry(all_tuples_, contact);
    // empty contact list
    // for flood, no need to maintain bundle list
    contact->bundle_list()->clear();

    
    //dont close the contact -- we have long running ones
    //this will PANIC
    //contact->close();
}

void
FloodBundleRouter::handle_route_add(RouteAddEvent* event,
                               BundleActionList* actions)
{
    BundleRouter::handle_route_add(event, actions);     
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
    log_debug("FLOOD:  new_next_hop");

    Bundle* bundle;
    BundleList::iterator iter;

    ScopeLock lock(pending_bundles_->lock());
    
    // upon arrival of new contact, send all pending bundles over contact
    for (iter = pending_bundles_->begin(); 
         iter != pending_bundles_->end(); ++iter) {
        bundle = *iter;
        actions->push_back(
            new BundleEnqueueAction(bundle, next_hop, FORWARD_COPY));
    }
}


int
FloodBundleRouter::fwd_to_matching(
                    Bundle* bundle, BundleActionList* actions, bool include_local)
{
    RouteEntry* entry;
    RouteEntrySet matches;
    RouteEntrySet::iterator iter;

    route_table_->get_matching(bundle->dest_, &matches);
    
    int count = 0;
    for (iter = matches.begin(); iter != matches.end(); ++iter) {
        entry = *iter;
        log_info("\tentry: point:%s --> %s [%s] local:%d",
                entry->pattern_.c_str(),
                entry->next_hop_->dest_tuple()->c_str(),
                bundle_fwd_action_toa(entry->action_),
                entry->next_hop_->is_local());
        if (!entry->next_hop_->is_local())
            continue;
        
        actions->push_back(
            new BundleEnqueueAction(bundle, entry->next_hop_, entry->action_));
        ++count;
        bundle->add_pending();
    }

    log_info("FLOOD: local_fwd_to_matching %s: %d matches", bundle->dest_.c_str(), count);
    return count;
}

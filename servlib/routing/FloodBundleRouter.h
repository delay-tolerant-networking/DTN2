#ifndef _FLOOD_BUNDLE_ROUTER_H_
#define _FLOOD_BUNDLE_ROUTER_H_

#include "BundleRouter.h"

/**
 * This is the implementation of the basic bundle routing algorithm
 * that only does flood routing. Routes can be parsed from a
 * configuration file or injected via the command line and/or
 * management interfaces.
 *
 * As a result, the class simply uses the default base class handlers
 * for all bundle events that flow through it, and the only code here
 * is that which parses the flood route configuration file.
 */
 
class FloodBundleRouter : public BundleRouter {
public:
    FloodBundleRouter();
    ~FloodBundleRouter();
    
    const int id() { return id_;}
protected:
    int id_;
    /**
     * Default event handler for new bundle arrivals.
     *
     * Queue the bundle on the pending delivery list, and then
     * searches through the route table to find any matching next
     * contacts, filling in the action list with forwarding decisions.
     */
    void handle_bundle_received(BundleReceivedEvent* event,
                                BundleActionList* actions);
    
    /**
     * Default event handler when bundles are transmitted.
     *
     * If the bundle was only partially transmitted, this calls into
     * the fragmentation module to create a new bundle fragment and
     * enqeues the new fragment on the appropriate list.
     */
    void handle_bundle_transmitted(BundleTransmittedEvent* event,
                                   BundleActionList* actions);

    /**
     * Default event handler when a new application registration
     * arrives.
     *
     * Adds an entry to the route table for the new registration, then
     * walks the pending list to see if any bundles match the
     * registration.
     */
    void handle_registration_added(RegistrationAddedEvent* event,
                                   BundleActionList* actions);
    
    /**
     * Default event handler when a new contact is available.
     */
    void handle_contact_available(ContactAvailableEvent* event,
                                  BundleActionList* actions);
    
    /**
     * Default event handler when a contact is broken.
     */
    void handle_contact_broken(ContactBrokenEvent* event,
                               BundleActionList* actions);
    
    /**
     * Default event handler when a new route is added by the command
     * or management interface.
     *
     * Adds an entry to the route table, then walks the pending list
     * to see if any bundles match the new route.
     */
    void handle_route_add(RouteAddEvent* event,
                          BundleActionList* actions);
    
    /**
     * Called whenever a new consumer (i.e. registration or contact)
     * arrives. This should walk the list of unassigned bundles (or
     * maybe all bundles???) to see if the new consumer matches.
     */
    void new_next_hop(const BundleTuplePattern& dest,
                              BundleConsumer* next_hop,
                              BundleActionList* actions);


    int fwd_to_matching(Bundle *bundle, BundleActionList *actions,bool include_local);

protected:
    BundleTuplePattern all_tuples_;
};

#endif /* _FLOOD_BUNDLE_ROUTER_H_ */

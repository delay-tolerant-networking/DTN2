#ifndef _BUNDLE_ROUTER_H_
#define _BUNDLE_ROUTER_H_

#include "bundling/BundleAction.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleTuple.h"
#include "debug/Logger.h"
#include "thread/Thread.h"
#include "util/StringUtils.h"
#include <vector>

class BundleConsumer;
class BundleRouter;
class RouteTable;
class StringBuffer;

/**
 * Typedef for a list of bundle routers.
 */
typedef std::vector<BundleRouter*> BundleRouterList;

/**
 * The BundleRouter is the main decision maker for all routing
 * decisions related to bundles.
 *
 * It runs receives events from the BundleForwarder having been
 * enqueued by other components. These events include all actions and
 * operations that may affect bundle delivery, including new bundle
 * arrival, contact arrival, etc.
 *
 * In response to each event the router generates a list of forwarding
 * actions that are then effected by the BundleForwarder.
 *
 * To support the prototyping of different routing protocols and
 * frameworks, the base class has a list of prospective BundleRouter
 * implementations, and at boot time, one of these is selected as the
 * active routing algorithm, while others can be selected as "shadow
 * routers", i.e. they get a copy of every event, and can generate
 * their own actions, but the forwarder doesn't actually perform any
 * actions. XXX/demmer spec this out more.
 *
 * As new algorithms are added to the system, new cases should be
 * added to the "create_router" function.
 */
class BundleRouter : public Logger {
public:
    /**
     * Factory method to create the correct subclass of BundleRouter
     * for the registered algorithm type.
     */
    static BundleRouter* create_router(const char* type);

    /**
     * Config variable for the active router type.
     */
    static std::string type_;
    
    /**
     * Constructor
     */
    BundleRouter();

    /**
     * Destructor
     */
    virtual ~BundleRouter();

    /**
     * Accessor for the route table.
     */
    const RouteTable* route_table() { return route_table_; }

    /**
     * Accessor for the pending bundles list.
     */
    const BundleList* pending_bundles() { return pending_bundles_; }
    
    /**
     * The monster routing decision function that is called in
     * response to all received events.
     *
     * To actually effect actions, this function should populate the
     * given action list with all forwarding decisions. The
     * BundleForwarder then takes these instructions and causes them
     * to happen.
     *
     * The base class implementation does a dispatch on the event type
     * and calls the appropriate default handler function.
     */
    virtual void handle_event(BundleEvent* event,
                              BundleActionList* actions);

    // XXX/demmer temp for testing fragmentation
    static size_t proactive_frag_threshold_;

    /**
     * The set of local regions that this router is configured as "in".
     */
    static StringVector local_regions_;

    /**
     * The default tuple for reaching this router, used for bundle
     * status reports, etc. Note that the region must be one of the
     * locally configured regions.
     */
    static BundleTuple local_tuple_;
    
protected:
    /**
     * Default event handler for new bundle arrivals.
     *
     * Queue the bundle on the pending delivery list, and then
     * searches through the route table to find any matching next
     * contacts, filling in the action list with forwarding decisions.
     */
    virtual void handle_bundle_received(BundleReceivedEvent* event,
                                        BundleActionList* actions);
    
    /**
     * Default event handler when bundles are transmitted.
     *
     * If the bundle was only partially transmitted, this calls into
     * the fragmentation module to create a new bundle fragment and
     * enqeues the new fragment on the appropriate list.
     */
    virtual void handle_bundle_transmitted(BundleTransmittedEvent* event,
                                           BundleActionList* actions);

    /**
     * Default event handler when a new application registration
     * arrives.
     *
     * Adds an entry to the route table for the new registration, then
     * walks the pending list to see if any bundles match the
     * registration.
     */
    virtual void handle_registration_added(RegistrationAddedEvent* event,
                                           BundleActionList* actions);
    
    /**
     * Default event handler when a new contact is available.
     */
    virtual void handle_contact_available(ContactAvailableEvent* event,
                                          BundleActionList* actions);
    
    /**
     * Default event handler when a contact is broken.
     */
    virtual void handle_contact_broken(ContactBrokenEvent* event,
                                       BundleActionList* actions);
    
    /**
     * Default event handler when reassembly is completed. For each
     * bundle on the list, check the pending count to see if the
     * fragment can be deleted.
     */
    virtual void handle_reassembly_completed(ReassemblyCompletedEvent* event,
                                             BundleActionList* actions);
    
    /**
     * Default event handler when a new route is added by the command
     * or management interface.
     *
     * Adds an entry to the route table, then walks the pending list
     * to see if any bundles match the new route.
     */
    virtual void handle_route_add(RouteAddEvent* event,
                                  BundleActionList* actions);
    
    /**
     * Get all matching entries from the routing table, and add a
     * corresponding forwarding action to the action list.
     *
     * Note that if the include_local flag is set, then local routes
     * (i.e. registrations) are included in the list.
     *
     * Returns the number of matches found and assigned.
     */
    virtual int fwd_to_matching(Bundle* bundle, BundleActionList* actions,
                        bool include_local);
    
    /**
     * Called whenever a new consumer (i.e. registration or contact)
     * arrives. This should walk the list of unassigned bundles (or
     * maybe all bundles???) to see if the new consumer matches.
     */
    virtual void new_next_hop(const BundleTuplePattern& dest,
                              BundleConsumer* next_hop,
                              BundleActionList* actions);

    /**
     * Delete the given bundle from the pending list (assumes the
     * pending count is zero).
     */
    void delete_from_pending(Bundle* bundle,
                             BundleActionList* actions);

    /// The routing table
    RouteTable* route_table_;

    /// The list of all bundles still pending delivery
    BundleList* pending_bundles_;

    /// The list of all bundles that I have custody of
    BundleList* custody_bundles_;
};
 
#endif /* _BUNDLE_ROUTER_H_ */

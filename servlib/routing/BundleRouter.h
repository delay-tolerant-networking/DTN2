#ifndef _BUNDLE_ROUTER_H_
#define _BUNDLE_ROUTER_H_

#include "bundling/BundleAction.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleTuple.h"
#include "debug/Logger.h"
#include "thread/Thread.h"
#include "thread/MsgQueue.h"
#include <set>
#include <vector>

class BundleConsumer;
class BundleEvent;
class BundleRouter;
class StringBuffer;

/**
 * Typedef for a list of bundle routers.
 */
typedef std::vector<BundleRouter*> BundleRouterList;

/**
 * The BundleRouter is the main decision maker for all routing
 * decisions related to bundles.
 *
 * It runs in its own thread that receives events from other parts of
 * the system. These events include all actions and operations that
 * may affect bundle delivery, including new bundle arrival, contact
 * establishment, etc.
 *
 * In response to these events, the router generates a list of
 * forwarding actions that can be fed to BundleForwarder::action().
 *
 * To support the prototyping of different routing protocols and
 * frameworks, the base class contains a list of registered
 * BundleRouter implementations. While the first entry of the list is
 * the only one whose action lists are sent to the forwarding agent,
 * all implementations will be forwarded a copy of each event. This
 * enables new prototypes to be compared side-by-side.
 *
 * The static dispatch_event function copies the event data to each
 * registered router.
 */
class BundleRouter : public Thread, public Logger {
public:
    /**
     * Add a new router algorithm to the list.
     */
    static void register_router(BundleRouter* router);
    
    /**
     * Return the vector of all registered routers.
     */
    static BundleRouterList *routers()
    {
        return &routers_;
    }

    /**
     * Dispatch the event to each registered router.
     */
    static void dispatch(BundleEvent* event);

    /**
     * Constructor
     */
    BundleRouter();
    
    /**
     * Add a route entry.
     */
    virtual bool add_route(const BundleTuplePattern& dest,
                           BundleConsumer* next_hop,
                           bundle_action_t action,
                           int argc = 0, const char** argv = 0);
    
    /**
     * Remove a route entry.
     */
    virtual bool del_route(const BundleTuplePattern& dest,
                           BundleConsumer* next_hop);

    /**
     * Dump a string representation of the routing table.
     */
    virtual void dump(StringBuffer* buf);
    
protected:
    /**
     * Structure for a route entry.
     */
    struct RouteEntry {
        RouteEntry(const BundleTuplePattern& pattern,
                   BundleConsumer* next_hop,
                   bundle_action_t action)
            : pattern_(pattern), action_(action), next_hop_(next_hop) {}

        /// The pattern to match in the route entry
        BundleTuplePattern pattern_;

        /// Forwarding action code 
        bundle_action_t action_;
        
        /// Next hop (registration or contact).
        BundleConsumer* next_hop_;
    };
    
    /**
     * The monster routing decision function that is called in
     * response to all received events. To cause bundles to be
     * forwarded, this function populates the given action list with
     * the forwarding decisions. The run() method takes the decisions
     * from the active router and passes them to the BundleForwarder
     * module.
     *
     * Note that the function is virtual so more complex derived
     * classes can override or augment the default behavior.
     */
    virtual void handle_event(BundleEvent* event,
                              BundleActionList* actions);

    /**
     * Loop through the routing table, adding an entry for each match
     * to the action list.
     */
    void get_matching(Bundle* bundle, BundleActionList* actions);
     

    /**
     * Called whenever a new consumer (i.e. registration or contact)
     * arrives. This should walk the list of unassigned bundles (or
     * maybe all bundles???) to see if the new consumer matches.
     */
    virtual void handle_next_hop(const BundleTuplePattern& dest,
                                 BundleConsumer* next_hop,
                                 BundleActionList* actions);
    
    /**
     * The main run loop.
     */
    void run();

    /**
     * Return true if this router is the active one.
     */
    bool active() { return routers_[0] == this; }

protected:
    /// The list of registered routers
    static BundleRouterList routers_;

    /// The main event queue
    MsgQueue<BundleEvent*> eventq_;

    /// The routing table
    typedef std::set<RouteEntry*> RouteTable;
    RouteTable route_table_;

    /// The list of all bundles still pending delivery
    BundleList* pending_bundles_;

    /// The list of all bundles that I have custody of
    BundleList* custody_bundles_;
};
 
#endif /* _BUNDLE_ROUTER_H_ */

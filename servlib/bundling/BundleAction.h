#ifndef _BUNDLE_ACTION_H_
#define _BUNDLE_ACTION_H_

#include <vector>
#include "Bundle.h"
#include "BundleMapping.h"
#include "BundleRef.h"

class BundleConsumer;

/**
 * Type code for bundling actions.
 */
typedef enum {
    ENQUEUE_BUNDLE,	///< Queue a bundle on a consumer
    DEQUEUE_BUNDLE,	///< Remove a bundle from a consumer

    STORE_ADD,		///< Store the bundle in the database
    STORE_DEL,		///< Remove the bundle from the database
} bundle_action_t;


/**
 * Conversion function from an action to a string.
 */
inline const char*
bundle_action_toa(bundle_action_t action)
{
    switch(action) {

    case ENQUEUE_BUNDLE:	return "ENQUEUE_BUNDLE";
    case DEQUEUE_BUNDLE:	return "DEQUEUE_BUNDLE";
        
    case STORE_ADD:		return "STORE_ADD";
    case STORE_DEL:		return "STORE_DEL";
        
    default:			return "(invalid action type)";
    }
}

/**
 * Structure encapsulating a generic bundle related action.
 */
class BundleAction {
public:
    BundleAction(bundle_action_t action, Bundle* bundle)
        : action_(action),
          bundleref_(bundle, "BundleAction", bundle_action_toa(action))
    {
    }
                 
    bundle_action_t action_;	///< action type code
    BundleRef bundleref_;	///< relevant bundle

private:
    /**
     * Make sure the action copy constructor is never run by making it
     * private, since it would screw up the Bundle reference counts.
     */
    BundleAction(const BundleAction& other);
};
    
/**
 * The vector of actions that is constructed by the BundleRouter.
 * Define a (empty) class, not a typedef, so we can use forward
 * declarations.
 */
class BundleActionList : public std::vector<BundleAction*> {
};

/**
 * Structure encapsulating an enqueue action.
 */
class BundleEnqueueAction : public BundleAction {
public:
    BundleEnqueueAction(Bundle* bundle,
                        BundleConsumer* nexthop,
                        bundle_fwd_action_t fwdaction,
                        int mapping_grp = 0,
                        u_int32_t expiration = 0,
                        RouterInfo* router_info = 0)
        
        : BundleAction(ENQUEUE_BUNDLE, bundle),
          nexthop_(nexthop)
    {
        mapping_.action_ 	= fwdaction;
        mapping_.mapping_grp_ 	= mapping_grp;
        mapping_.timeout_ 	= expiration;
        mapping_.router_info_ 	= router_info;
    }

    BundleConsumer* nexthop_;		///< destination
    BundleMapping mapping_;		///< mapping structure
};

/**
 * Structure for a dequeue action.
 */
class BundleDequeueAction : public BundleAction {
    BundleDequeueAction(Bundle* bundle,
                        BundleConsumer* nexthop)
        : BundleAction(DEQUEUE_BUNDLE, bundle),
          nexthop_(nexthop)
    {
    }
        
    BundleConsumer* nexthop_;		///< destination
};

    

#endif /* _BUNDLE_ACTION_H_ */

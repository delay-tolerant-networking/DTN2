#ifndef _BUNDLE_ACTION_H_
#define _BUNDLE_ACTION_H_

#include <vector>
#include "Bundle.h"
#include "BundleRef.h"

class BundleConsumer;

/**
 * Type code for bundling actions.
 */
typedef enum {
    FORWARD_UNIQUE,	///< Forward the bundle
    FORWARD_COPY,	///< Forward a copy of the bundle
    FORWARD_CANCEL,	///< Cancel a requested transmissision

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
    case FORWARD_UNIQUE:	return "FORWARD_UNIQUE";
    case FORWARD_COPY:		return "FORWARD_COPY";
    case FORWARD_CANCEL:	return "FORWARD_CANCEL";

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
        : action_(action), bundleref_(bundle) {}
                 
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
 * Structure encapsulating a forwarding action.
 */
class BundleForwardAction : public BundleAction {
public:
    BundleForwardAction(bundle_action_t action, Bundle* bundle,
                        BundleConsumer* nexthop)
        : BundleAction(action, bundle), nexthop_(nexthop) {}
    
    BundleConsumer* nexthop_;	///< destination
};
    
/**
 * The vector of actions that is constructed by the BundleRouter.
 * Define a (empty) class, not a typedef, so we can use forward
 * declarations.
 */
class BundleActionList : public std::vector<BundleAction*> {
};

#endif /* _BUNDLE_ACTION_H_ */

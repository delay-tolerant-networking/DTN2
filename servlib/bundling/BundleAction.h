#ifndef _BUNDLE_ACTION_H_
#define _BUNDLE_ACTION_H_

#include <vector>
#include "Bundle.h"
#include "BundleRef.h"

class BundleConsumer;

/**
 * Type code for bundling forwarding actions.
 */
typedef enum {
    FORWARD_UNIQUE,		///< Forward the bundle
    FORWARD_COPY,		///< Forward a copy of the bundle
    CANCEL			///< Cancel a previously requested transmissision
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
    case CANCEL:		return "CANCEL";
    default:			return "(invalid action type)";
    }
}

/**
 * Structure encapsulating forwarding actions for an associated
 * bundle.
 */
class BundleAction {
public:
    BundleAction(bundle_action_t action, Bundle* bundle,
                 BundleConsumer* nexthop);
                 
    bundle_action_t action_;	///< action type code
    BundleRef bundleref_;	///< relevant bundle
    BundleConsumer* nexthop_;	///< destination
    size_t start_;		///< start of data range
    size_t end_;		///< end of data range

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
 * BundleAction constructor.
 */
inline
BundleAction::BundleAction(bundle_action_t action, Bundle* bundle,
                           BundleConsumer* nexthop)
    : action_(action), bundleref_(bundle), nexthop_(nexthop)
{
}

#endif /* _BUNDLE_ACTION_H_ */

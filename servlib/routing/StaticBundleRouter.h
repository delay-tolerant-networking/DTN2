#ifndef _STATIC_BUNDLE_ROUTER_H_
#define _STATIC_BUNDLE_ROUTER_H_

#include "BundleRouter.h"

/**
 * This is the implementation of the basic bundle routing algorithm
 * that only does static routing. Routes can be parsed from a
 * configuration file or injected via the command line and/or
 * management interfaces.
 *
 * As a result, the class simply uses the default base class handlers
 * for all bundle events that flow through it, and the only code here
 * is that which parses the static route configuration file.
 */
 
class StaticBundleRouter : public BundleRouter {
public:
    StaticBundleRouter() {}
};

#endif /* _STATIC_BUNDLE_ROUTER_H_ */

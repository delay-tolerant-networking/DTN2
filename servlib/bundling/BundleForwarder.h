#ifndef _BUNDLE_FORWARDING_H_
#define _BUNDLE_FORWARDING_H_

#include "debug/Log.h"
#include <vector>

class BundleActionList;
class Bundle;
class BundleConsumer;

/**
 * Class that handles the flow of bundles through the system.
 */
class BundleForwarder : public Logger {
public:
    /**
     * Singleton accessor.
     */
    static BundleForwarder* instance() {
        ASSERT(instance_ != NULL);
        return instance_;
    }

    /**
     * Boot time initializer.
     */
    static void init();

    /**
     * Constructor.
     */
    BundleForwarder();

    /**
     * Routine that actually effects the forwarding operations as
     * returned from the BundleRouter.
     */
    void process(BundleActionList* actions);

protected:
    static BundleForwarder* instance_;
};

#endif /* _BUNDLE_FORWARDING_H_ */

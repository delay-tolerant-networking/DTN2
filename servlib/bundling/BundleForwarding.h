#ifndef _BUNDLE_FORWARDING_H_
#define _BUNDLE_FORWARDING_H_

#include "debug/Log.h"

class Bundle;

/**
 * Class that handles the flow of bundles through the system.
 */
class BundleForwarding : public Logger {
public:
    static BundleForwarding* instance() {
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
    BundleForwarding();

    /**
     * Common input routine for incoming bundles.
     */
    void input(Bundle* bundle);

protected:
    static BundleForwarding* instance_;
};

#endif /* _BUNDLE_FORWARDING_H_ */

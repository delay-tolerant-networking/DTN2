#ifndef _BUNDLE_FORWARDING_H_
#define _BUNDLE_FORWARDING_H_

#include "debug/Log.h"
#include "thread/Thread.h"
#include "thread/MsgQueue.h"
#include <vector>

class Bundle;
class BundleAction;
class BundleActionList;
class BundleConsumer;
class BundleEvent;
class BundleRouter;
class StringBuffer;

/**
 * Class that handles the flow of bundle events through the system.
 */
class BundleForwarder : public Logger, public Thread {
public:
    /**
     * Singleton accessor.
     */
    static BundleForwarder* instance() {
        ASSERT(instance_ != NULL);
        return instance_;
    }

    /**
     * Constructor.
     */
    BundleForwarder();

    /**
     * Boot time initializer.
     */
    static void init(BundleForwarder* instance)
    {
        if (instance_ != NULL) {
            PANIC("BundleForwarder already initialized");
        }
        instance_ = instance;
    }
    
    /**
     * Queues the given event on the pending list of events.
     */
    static void post(BundleEvent* event);

    /**
     * Returns the currently active bundle router.
     */
    BundleRouter* active_router() { return router_; }

    /**
     * Sets the active router.
     */
    void set_active_router(BundleRouter* router) { router_ = router; }

    /**
     * Format the given StringBuffer with the current statistics value.
     */
    void get_statistics(StringBuffer* buf);

protected:
    /**
     * Routine that implements a particular action, as returned from
     * the BundleRouter.
     */
    void process(BundleAction* action);

    /**
     * Main thread function that dispatches events.
     */
    void run();
    
    /// The active bundle router
    BundleRouter* router_;

    /// The event queue
    MsgQueue<BundleEvent*> eventq_;

    /// Statistics
    u_int32_t bundles_received_;
    u_int32_t bundles_sent_local_;
    u_int32_t bundles_sent_remote_;
    u_int32_t bundles_expired_;
    
    static BundleForwarder* instance_;
};

#endif /* _BUNDLE_FORWARDING_H_ */

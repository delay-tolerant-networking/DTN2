#ifndef _ADMIN_REGISTRATION_H_
#define _ADMIN_REGISTRATION_H_

#include "Registration.h"

/**
 * Internal registration that recieves all administrative bundles
 * destined for the router itself (i.e. status reports, custody
 * acknowledgements, ping bundles, etc.)
*/
class AdminRegistration : public Registration {
public:
    AdminRegistration();

    /**
     * Consume the given bundle, queueing it if required.
     */
    void enqueue_bundle(Bundle* bundle, const BundleMapping* mapping);

    /**
     * Attempt to remove the given bundle from the queue.
     *
     * @return true if the bundle was dequeued, false if not.
     */
    bool dequeue_bundle(Bundle* bundle, BundleMapping** mappingp);

    /**
     * Check if the given bundle is already queued on this consumer.
     */
    bool is_queued(Bundle* bundle);
};

#endif /* _ADMIN_REGISTRATION_H_ */

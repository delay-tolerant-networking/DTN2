#ifndef _BUNDLE_EVENT_H_
#define _BUNDLE_EVENT_H_

#include "BundleRef.h"

/**
 * All signaling from various components to the routing layer is done
 * via the Bundle Event message abstraction. This file defines the
 * event type codes and corresponding classes.
 */

class Bundle;
class BundleConsumer;
class Contact;
class Registration;

/**
 * Type codes for events.
 */
typedef enum {
    BUNDLE_RECEIVED = 0x1,	///< New bundle arrival
    BUNDLE_TRANSMITTED,		///< Bundle or fragment successfully sent
    BUNDLE_EXPIRED,		///< Bundle expired
        
    CONTACT_AVAILABLE,		///< Contact is available
    CONTACT_BROKEN,		///< Contact abnormally terminated
        
    REGISTRATION_ADDED,		///< New registration arrived
    REGISTRATION_REMOVED,	///< Registration removed
    REGISTRATION_EXPIRED,	///< Registration expired
        
} event_type_t;

/**
 * Event base class.
 */
class BundleEvent {
public:
    /**
     * The event type code;
     */
    const event_type_t type_;

    /**
     * Need a virtual destructor to make sure all the right bits are
     * cleaned up.
     */
    virtual ~BundleEvent() {}
    
protected:
    /**
     * Constructor (protected since one of the subclasses should
     * always be that which is actually initialized.
     */
    BundleEvent(event_type_t type) : type_(type) {};
    
};

/**
 * Event class for new bundle arrivals.
 */
class BundleReceivedEvent : public BundleEvent {
public:
    BundleReceivedEvent(Bundle* bundle)
        : BundleEvent(BUNDLE_RECEIVED),
          bundleref_(bundle) {}

    /// The newly arrived bundle
    BundleRef bundleref_;
};

/**
 * Event class for bundle or fragment transmission.
 */
class BundleTransmittedEvent : public BundleEvent {
public:
    BundleTransmittedEvent(Bundle* bundle, BundleConsumer* consumer,
                           size_t bytes_sent, bool acked)
        : BundleEvent(BUNDLE_TRANSMITTED),
          bundleref_(bundle), consumer_(consumer),
          bytes_sent_(bytes_sent), acked_(acked) {}
    
    /// The transmitted bundle
    BundleRef bundleref_;

    /// The contact or registration where the bundle was sent
    BundleConsumer* consumer_;

    /// Total number of bytes sent
    size_t bytes_sent_;

    /// Indication if the destination acknowledged bundle receipt
    bool acked_;
};

/**
 * Event class for new registration arrivals.
 */
class RegistrationAddedEvent : public BundleEvent {
public:
    RegistrationAddedEvent(Registration* reg)
        : BundleEvent(REGISTRATION_ADDED), registration_(reg) {}

    /// The newly added registration
    Registration* registration_;
};


#endif /* _BUNDLE_EVENT_H_ */

#ifndef _BUNDLE_EVENT_H_
#define _BUNDLE_EVENT_H_

/**
 * All signaling from various components to the routing layer is done
 * via the Bundle Event message abstraction. This file defines the
 * event type codes and corresponding classes.
 */

class Bundle;
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
     * The type code of the event.
     */
    const event_type_t type_;		///< The event type code

protected:
    /**
     * Constructor (protected since one of the subclasses should
     * always be that which is actually initialized.
     */
    BundleEvent(event_type_t type) : type_(type) {};
    virtual ~BundleEvent() {}
    
};

/**
 * Event class for new bundle arrivals.
 */
class BundleReceivedEvent : public BundleEvent {
public:
    BundleReceivedEvent(Bundle* bundle) :
        BundleEvent(BUNDLE_RECEIVED), bundle_(bundle) {}
    ~BundleReceivedEvent() {}

    /// The newly arrived bundle
    Bundle* bundle_;
};

/**
 * Event class for new registration arrivals.
 */
class RegistrationAddedEvent : public BundleEvent {
public:
    RegistrationAddedEvent(Registration* reg)
        : BundleEvent(REGISTRATION_ADDED), registration_(reg) {}
    ~RegistrationAddedEvent() {}

    /// The newly added registration
    Registration* registration_;
};


#endif /* _BUNDLE_EVENT_H_ */

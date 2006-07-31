/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 *
 * Intel Open Source License
 *
 * Copyright (c) 2004 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _BUNDLE_EVENT_H_
#define _BUNDLE_EVENT_H_

#include "Bundle.h"
#include "BundleRef.h"
#include "BundleList.h"
#include "CustodySignal.h"
#include "contacts/Link.h"

namespace dtn {

/**
 * All signaling from various components to the routing layer is done
 * via the Bundle Event message abstraction. This file defines the
 * event type codes and corresponding classes.
 */

class Bundle;
class Contact;
class Registration;
class RouteEntry;
class Link;

/**
 * Type codes for events / requests.
 */
typedef enum {
    BUNDLE_RECEIVED = 0x1,	///< New bundle arrival
    BUNDLE_TRANSMITTED,		///< Bundle or fragment successfully sent
    BUNDLE_TRANSMIT_FAILED,	///< Bundle or fragment successfully sent
    BUNDLE_DELIVERED,		///< Bundle locally delivered
    BUNDLE_EXPIRED,		///< Bundle expired
    BUNDLE_FREE,		///< No more references to the bundle
    BUNDLE_FORWARD_TIMEOUT,	///< A Mapping timed out

    CONTACT_UP,		        ///< Contact is up
    CONTACT_DOWN,		///< Contact abnormally terminated

    LINK_CREATED,		///< Link is created into the system
    LINK_DELETED,		///< Link is deleted from the system
    LINK_AVAILABLE,		///< Link is available
    LINK_UNAVAILABLE,		///< Link is unavailable

    LINK_STATE_CHANGE_REQUEST,	///< Link state should be changed

    REASSEMBLY_COMPLETED,	///< Reassembly completed

    REGISTRATION_ADDED,		///< New registration arrived
    REGISTRATION_REMOVED,	///< Registration removed
    REGISTRATION_EXPIRED,	///< Registration expired

    ROUTE_ADD,			///< Add a new entry to the route table
    ROUTE_DEL,			///< Remove an entry from the route table

    CUSTODY_SIGNAL,		///< Custody transfer signal received
    CUSTODY_TIMEOUT,		///< Custody transfer timer fired

    DAEMON_SHUTDOWN,            ///< Shut the daemon down cleanly
    DAEMON_STATUS,		///< No-op event to check the daemon

} event_type_t;

/**
 * Conversion function from an event to a string.
 */
inline const char*
event_to_str(event_type_t event)
{
    switch(event) {

    case BUNDLE_RECEIVED:	return "BUNDLE_RECEIVED";
    case BUNDLE_TRANSMITTED:	return "BUNDLE_TRANSMITTED";
    case BUNDLE_TRANSMIT_FAILED:return "BUNDLE_TRANSMIT_FAILED";
    case BUNDLE_DELIVERED:	return "BUNDLE_DELIVERED";
    case BUNDLE_EXPIRED:	return "BUNDLE_EXPIRED";
    case BUNDLE_FREE:		return "BUNDLE_FREE";
    case BUNDLE_FORWARD_TIMEOUT:return "BUNDLE_FORWARD_TIMEOUT";

    case CONTACT_UP:		return "CONTACT_UP";
    case CONTACT_DOWN:		return "CONTACT_DOWN";

    case LINK_CREATED:		return "LINK_CREATED";
    case LINK_DELETED:		return "LINK_DELETED";
    case LINK_AVAILABLE:	return "LINK_AVAILABLE";
    case LINK_UNAVAILABLE:	return "LINK_UNAVAILABLE";

    case LINK_STATE_CHANGE_REQUEST:return "LINK_STATE_CHANGE_REQUEST";

    case REASSEMBLY_COMPLETED:	return "REASSEMBLY_COMPLETED";

    case REGISTRATION_ADDED:	return "REGISTRATION_ADDED";
    case REGISTRATION_REMOVED:	return "REGISTRATION_REMOVED";
    case REGISTRATION_EXPIRED:	return "REGISTRATION_EXPIRED";

    case ROUTE_ADD:		return "ROUTE_ADD";
    case ROUTE_DEL:		return "ROUTE_DEL";

    case CUSTODY_SIGNAL:	return "CUSTODY_SIGNAL";
    case CUSTODY_TIMEOUT:	return "CUSTODY_TIMEOUT";
    
    case DAEMON_SHUTDOWN:	return "SHUTDOWN";
    case DAEMON_STATUS:		return "DAEMON_STATUS";
        
    default:			return "(invalid event type)";
    }
}

/**
 * Possible sources for events.
 */
typedef enum {
    EVENTSRC_PEER  = 1,		///< a peer router
    EVENTSRC_APP   = 2,		///< a local application
    EVENTSRC_STORE = 3,		///< the data store
    EVENTSRC_ADMIN = 4,		///< the admin logic
    EVENTSRC_FRAGMENTATION = 5	///< the fragmentation engine
} event_source_t;

/**
 * Event base class.
 */
class BundleEvent {
public:
    /**
     * The event type code.
     */
    const event_type_t type_;

    /**
     * Bit indicating whether this event is for the daemon only or if
     * it should be propagated to other components (i.e. the various
     * routers).
     */
    bool daemon_only_;

    /**
     * Slot for a notifier to indicate that the event was processed.
     */
    oasys::Notifier* processed_notifier_;

    /**
     * Used for printing
     */
    const char* type_str() {
        return event_to_str(type_);
    }

    /**
     * Need a virtual destructor to make sure all the right bits are
     * cleaned up and to notify any threads that are waiting for the
     * event to be processed.
     */
    virtual ~BundleEvent()
    {
        if (processed_notifier_) {
            processed_notifier_->notify();
        }
    }

protected:
    /**
     * Constructor (protected since one of the subclasses should
     * always be that which is actually initialized.
     */
    BundleEvent(event_type_t type)
        : type_(type), daemon_only_(false), processed_notifier_(false) {}
};

/**
 * Event class for new bundle arrivals.
 */
class BundleReceivedEvent : public BundleEvent {
public:
    /*
     * Constructor -- if the bytes_received is unspecified it is
     * assumed to be the length of the bundle.
     */
    BundleReceivedEvent(Bundle* bundle,
                        event_source_t source,
                        size_t bytes_received = 0)

        : BundleEvent(BUNDLE_RECEIVED),
          bundleref_(bundle, "BundleReceivedEvent"),
          source_(source)
    {
        if (bytes_received != 0)
            bytes_received_ = bytes_received;
        else
            bytes_received_ = bundle->payload_.length();
    }

    /// The newly arrived bundle
    BundleRef bundleref_;

    /// The source of the bundle
    event_source_t source_;

    /// The total bytes actually received
    size_t bytes_received_;
};

/**
 * Event class for bundle or fragment transmission.
 */
class BundleTransmittedEvent : public BundleEvent {
public:
    BundleTransmittedEvent(Bundle* bundle, const ContactRef& contact,
                           size_t bytes_sent, size_t reliably_sent)
        : BundleEvent(BUNDLE_TRANSMITTED),
          bundleref_(bundle, "BundleTransmittedEvent"),
          contact_(contact.object(), "BundleTransmittedEvent"),
          bytes_sent_(bytes_sent),
          reliably_sent_(reliably_sent) {}

    /// The transmitted bundle
    BundleRef bundleref_;

    /// The contact where the bundle was sent
    ContactRef contact_;

    /// Total number of bytes sent
    size_t bytes_sent_;

    /// If the convergence layer that we sent on is reliable, this is
    /// the count of the bytes reliably sent, which must be less than
    /// or equal to the bytes transmitted
    size_t reliably_sent_;
};

/**
 * Event class for a failed transmission, which can occur if a link
 * closes after a router has issued a transmission request but before
 * the bundle is successfully sent.
 */
class BundleTransmitFailedEvent : public BundleEvent {
public:
    BundleTransmitFailedEvent(Bundle* bundle, const ContactRef& contact)
        : BundleEvent(BUNDLE_TRANSMIT_FAILED),
          bundleref_(bundle, "BundleTransmitFailedEvent"),
          contact_(contact.object(), "BundleTransmitFailedEvent") {}

    /// The transmitted bundle
    BundleRef bundleref_;

    /// The contact where the bundle was attempted to be sent
    ContactRef contact_;
};

/**
 * Event class for local bundle delivery.
 */
class BundleDeliveredEvent : public BundleEvent {
public:
    BundleDeliveredEvent(Bundle* bundle, Registration* registration)
        : BundleEvent(BUNDLE_DELIVERED),
          bundleref_(bundle, "BundleDeliveredEvent"),
          registration_(registration) {}


    /// The delivered bundle
    BundleRef bundleref_;

    /// The registration that got it
    Registration* registration_;
};

/**
 * Event class for bundle expiration.
 */
class BundleExpiredEvent : public BundleEvent {
public:
    BundleExpiredEvent(Bundle* bundle)
        : BundleEvent(BUNDLE_EXPIRED),
          bundleref_(bundle, "BundleExpiredEvent") {}

    /// The expired bundle
    BundleRef bundleref_;
};

/**
 * Event class for bundles that have no more references to them.
 */
class BundleFreeEvent : public BundleEvent {
public:
    BundleFreeEvent(Bundle* bundle)
        : BundleEvent(BUNDLE_FREE),
          bundle_(bundle)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
    
    /// The freed bundle
    Bundle* bundle_;
};

/**
 * Abstract class for the subset of events related to contacts and
 * links that defines a reason code enumerated type.
 */
class ContactEvent : public BundleEvent {
public:
    /**
     * Reason codes for contact state operations
     */
    typedef enum {
        INVALID = 0,	///< Should not be used
        NO_INFO,	///< No additional info
        USER,		///< User action (i.e. console / config)
        BROKEN,		///< Unexpected session interruption
        SHUTDOWN,	///< Clean connection shutdown
        RECONNECT,	///< Re-establish link after failure
        IDLE,		///< Idle connection shut down by the CL
        TIMEOUT,	///< Scheduled link ended duration
        UNBLOCKED	///< No longer busy
    } reason_t;

    /**
     * Reason to string conversion.
     */
    static const char* reason_to_str(reason_t reason) {
        switch(reason) {
        case INVALID:	return "INVALID";
        case NO_INFO:	return "no additional info";
        case USER: 	return "user action";
        case SHUTDOWN: 	return "peer shut down";
        case BROKEN:	return "connection broken";
        case RECONNECT:	return "re-establishing connection";
        case IDLE:	return "connection idle";
        case TIMEOUT:	return "schedule timed out";
        case UNBLOCKED:	return "no longer busy";
        }
        NOTREACHED;
    }

    /// Constructor
    ContactEvent(event_type_t type, reason_t reason = NO_INFO)
        : BundleEvent(type), reason_(reason) {}

    reason_t reason_;	///< reason code for the event
};

/**
 * Event class for contact up events
 */
class ContactUpEvent : public ContactEvent {
public:
    ContactUpEvent(const ContactRef& contact)
        : ContactEvent(CONTACT_UP),
          contact_(contact.object(), "ContactUpEvent") {}

    /// The contact that is up
    ContactRef contact_;
};

/**
 * Event class for contact down events
 */
class ContactDownEvent : public ContactEvent {
public:
    ContactDownEvent(const ContactRef& contact, reason_t reason)
        : ContactEvent(CONTACT_DOWN, reason),
          contact_(contact.object(), "ContactDownEvent") {}

    /// The contact that is now down
    ContactRef contact_;
};

/**
 * Event class for link creation events
 */
class LinkCreatedEvent : public ContactEvent {
public:
    LinkCreatedEvent(Link* link)
        : ContactEvent(LINK_CREATED, ContactEvent::USER), link_(link) {}

    Link* link_;
};

/**
 * Event class for link deletion events
 */
class LinkDeletedEvent : public ContactEvent {
public:
    LinkDeletedEvent(Link* link)
        : ContactEvent(LINK_DELETED, ContactEvent::USER), link_(link) {}

    /// The link that is up
    Link* link_;
};

/**
 * Event class for link available events
 */
class LinkAvailableEvent : public ContactEvent {
public:
    LinkAvailableEvent(Link* link, reason_t reason)
        : ContactEvent(LINK_AVAILABLE, reason), link_(link) {}

    Link* link_;
};

/**
 * Event class for link unavailable events
 */
class LinkUnavailableEvent : public ContactEvent {
public:
    LinkUnavailableEvent(Link* link, reason_t reason)
        : ContactEvent(LINK_UNAVAILABLE, reason), link_(link) {}

    /// The link that is up
    Link* link_;
};

/**
 * Request class for link state change requests that are sent to the
 * daemon thread for processing. This includes requests to open or
 * close the link, and changing its status to available or
 * unavailable.
 *
 * When closing a link, a reason must be given for the event.
 */
class LinkStateChangeRequest : public ContactEvent {
public:
    /// Shared type code for state_t with Link
    typedef Link::state_t state_t;

    LinkStateChangeRequest(Link* link, state_t state, reason_t reason)
        : ContactEvent(LINK_STATE_CHANGE_REQUEST, reason),
          link_(link), state_(state)
    {
        daemon_only_ = true;
    }

    /// The link to be changed
    Link* link_;

    /// Requested state
    state_t state_;
};

/**
 * Event class for new registration arrivals.
 */
class RegistrationAddedEvent : public BundleEvent {
public:
    RegistrationAddedEvent(Registration* reg, event_source_t source)
        : BundleEvent(REGISTRATION_ADDED), registration_(reg),
          source_(source) {}

    /// The newly added registration
    Registration* registration_;

    /// Why is the registration added
    event_source_t source_;
};

/**
 * Event class for registration removals.
 */
class RegistrationRemovedEvent : public BundleEvent {
public:
    RegistrationRemovedEvent(Registration* reg)
        : BundleEvent(REGISTRATION_REMOVED), registration_(reg) {}

    /// The to-be-removed registration
    Registration* registration_;
};

/**
 * Event class for registration expiration.
 */
class RegistrationExpiredEvent : public BundleEvent {
public:
    RegistrationExpiredEvent(u_int32_t regid)
        : BundleEvent(REGISTRATION_EXPIRED), regid_(regid) {}

    /// The to-be-removed registration id
    u_int32_t regid_;
};

/**
 * Event class for route add events
 */
class RouteAddEvent : public BundleEvent {
public:
    RouteAddEvent(RouteEntry* entry)
        : BundleEvent(ROUTE_ADD), entry_(entry) {}

    /// The route table entry to be added
    RouteEntry* entry_;
};

/**
 * Event class for route delete events
 */
class RouteDelEvent : public BundleEvent {
public:
    RouteDelEvent(const EndpointIDPattern& dest)
        : BundleEvent(ROUTE_DEL), dest_(dest) {}

    /// The destination eid to be removed
    EndpointIDPattern dest_;
};

/**
 * Event class for reassembly completion.
 */
class ReassemblyCompletedEvent : public BundleEvent {
public:
    ReassemblyCompletedEvent(Bundle* bundle, BundleList* fragments)
        : BundleEvent(REASSEMBLY_COMPLETED),
          bundle_(bundle, "ReassemblyCompletedEvent"),
          fragments_("ReassemblyCompletedEvent")
    {
        fragments->move_contents(&fragments_);
    }

    /// The newly reassembled bundle
    BundleRef bundle_;

    /// The list of bundle fragments
    BundleList fragments_;
};

/**
 * Event class for custody transfer signal arrivals.
 */
class CustodySignalEvent : public BundleEvent {
public:
    CustodySignalEvent(const CustodySignal::data_t& data)
        : BundleEvent(CUSTODY_SIGNAL), data_(data) {}
    
    /// The parsed data from the custody transfer signal
    CustodySignal::data_t data_;
};

/**
 * Event class for custody transfer timeout events
 */
class CustodyTimeoutEvent : public BundleEvent {
public:
    CustodyTimeoutEvent(Bundle* bundle, Link* link)
        : BundleEvent(CUSTODY_TIMEOUT),
          bundle_(bundle, "CustodyTimeoutEvent"),
          link_(link) {}

    ///< The bundle whose timer fired
    BundleRef bundle_;

    ///< The link it was sent on
    Link* link_;
};

/**
 * Event class for shutting down a daemon. The daemon goes through all
 * the links and call link->close() on them (if they're in one of the
 * open link states), then cleanly closes the various data
 * stores, then calls exit().
 */
class ShutdownRequest : public BundleEvent {
public:
    ShutdownRequest() : BundleEvent(DAEMON_SHUTDOWN)
    {
        daemon_only_ = true;
    }
};

/**
 * Event class for checking that the daemon is still running.
 */
class StatusRequest : public BundleEvent {
public:
    StatusRequest() : BundleEvent(DAEMON_STATUS)
    {
        daemon_only_ = true;
    }
};

} // namespace dtn

#endif /* _BUNDLE_EVENT_H_ */

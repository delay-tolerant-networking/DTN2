/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _BUNDLE_EVENT_H_
#define _BUNDLE_EVENT_H_

#include "Bundle.h"
#include "BundleRef.h"
#include "BundleList.h"
#include "CustodySignal.h"
#include "contacts/Link.h"

#include <oasys/serialize/Serialize.h>

namespace dtn {

/**
 * All signaling from various components to the routing layer is done
 * via the Bundle Event message abstraction. This file defines the
 * event type codes and corresponding classes.
 */

class Bundle;
class Contact;
class Interface;
class Registration;
class RouteEntry;

/**
 * Type codes for events / requests.
 */
typedef enum {
    BUNDLE_RECEIVED = 0x1,	///< New bundle arrival
    BUNDLE_TRANSMITTED,		///< Bundle or fragment successfully sent
    BUNDLE_TRANSMIT_FAILED,	///< Bundle or fragment successfully sent
    BUNDLE_DELIVERED,		///< Bundle locally delivered
    BUNDLE_DELIVERY,		///< Bundle delivery (with payload)
    BUNDLE_EXPIRED,		///< Bundle expired
    BUNDLE_FREE,		///< No more references to the bundle
    BUNDLE_FORWARD_TIMEOUT,	///< A Mapping timed out
    BUNDLE_SEND,                ///< Send a bundle
    BUNDLE_CANCEL,              ///< Cancel a bundle transmission
    BUNDLE_INJECT,              ///< Inject a bundle
    BUNDLE_ACCEPT_REQUEST,	///< Request acceptance of a new bundle
    BUNDLE_QUERY,               ///< Bundle query
    BUNDLE_REPORT,              ///< Response to bundle query

    CONTACT_UP,		        ///< Contact is up
    CONTACT_DOWN,		///< Contact abnormally terminated
    CONTACT_QUERY,              ///< Contact query
    CONTACT_REPORT,             ///< Response to contact query

    LINK_CREATED,		///< Link is created into the system
    LINK_DELETED,		///< Link is deleted from the system
    LINK_AVAILABLE,		///< Link is available
    LINK_UNAVAILABLE,		///< Link is unavailable
    LINK_BUSY,  		///< Link is busy 
    LINK_CREATE,                ///< Create and open a new link
    LINK_QUERY,                 ///< Link query
    LINK_REPORT,                ///< Response to link query

    LINK_STATE_CHANGE_REQUEST,	///< Link state should be changed

    REASSEMBLY_COMPLETED,	///< Reassembly completed

    REGISTRATION_ADDED,		///< New registration arrived
    REGISTRATION_REMOVED,	///< Registration removed
    REGISTRATION_EXPIRED,	///< Registration expired

    ROUTE_ADD,			///< Add a new entry to the route table
    ROUTE_DEL,			///< Remove an entry from the route table
    ROUTE_QUERY,                ///< Static route query
    ROUTE_REPORT,               ///< Response to static route query

    CUSTODY_SIGNAL,		///< Custody transfer signal received
    CUSTODY_TIMEOUT,		///< Custody transfer timer fired

    DAEMON_SHUTDOWN,            ///< Shut the daemon down cleanly
    DAEMON_STATUS,		///< No-op event to check the daemon

} event_type_t;

/**
 * Conversion function from an event to a string.
 */
inline const char*
event_to_str(event_type_t event, bool xml=false)
{
    switch(event) {

    case BUNDLE_RECEIVED:	return xml ? "bundle_received_event" : "BUNDLE_RECEIVED";
    case BUNDLE_TRANSMITTED:	return xml ? "bundle_transmitted_event" : "BUNDLE_TRANSMITTED";
    case BUNDLE_TRANSMIT_FAILED:return xml ? "bundle_transmit_failed_event" : "BUNDLE_TRANSMIT_FAILED";
    case BUNDLE_DELIVERED:	return "BUNDLE_DELIVERED";
    case BUNDLE_DELIVERY:	return xml ? "bundle_delivery_event" : "BUNDLE_DELIVERY";
    case BUNDLE_EXPIRED:	return xml ? "bundle_expired_event" : "BUNDLE_EXPIRED";
    case BUNDLE_FREE:		return "BUNDLE_FREE";
    case BUNDLE_FORWARD_TIMEOUT:return "BUNDLE_FORWARD_TIMEOUT";
    case BUNDLE_SEND:           return xml ? "send_bundle_action" : "BUNDLE_SEND";
    case BUNDLE_CANCEL:         return xml ? "cancel_bundle_action" : "BUNDLE_CANCEL";
    case BUNDLE_INJECT:         return xml ? "inject_bundle_action" : "BUNDLE_INJECT";
    case BUNDLE_ACCEPT_REQUEST:	return xml ? "bundle_accept_request" : "BUNDLE_ACCEPT_REQUEST";
    case BUNDLE_QUERY:          return xml ? "bundle_query" : "BUNDLE_QUERY";
    case BUNDLE_REPORT:         return xml ? "bundle_report" : "BUNDLE_REPORT";

    case CONTACT_UP:		return xml ? "contact_up_event" : "CONTACT_UP";
    case CONTACT_DOWN:		return xml ? "contact_down_event" : "CONTACT_DOWN";
    case CONTACT_QUERY:         return xml ? "contact_query" : "CONTACT_QUERY";
    case CONTACT_REPORT:        return xml ? "contact_report" : "CONTACT_REPORT";

    case LINK_CREATED:		return xml ? "link_created_event" : "LINK_CREATED";
    case LINK_DELETED:		return xml ? "link_deleted_event" : "LINK_DELETED";
    case LINK_AVAILABLE:	return xml ? "link_available_event" : "LINK_AVAILABLE";
    case LINK_UNAVAILABLE:	return xml ? "link_unavailable_event" : "LINK_UNAVAILABLE";
    case LINK_BUSY:     	return xml ? "link_busy_event" : "LINK_BUSY";
    case LINK_CREATE:           return "LINK_CREATE";
    case LINK_QUERY:            return xml ? "link_query" : "LINK_QUERY";
    case LINK_REPORT:           return xml ? "link_report" : "LINK_REPORT";

    case LINK_STATE_CHANGE_REQUEST:return "LINK_STATE_CHANGE_REQUEST";

    case REASSEMBLY_COMPLETED:	return "REASSEMBLY_COMPLETED";

    case REGISTRATION_ADDED:	return xml ? "registration_added_event" : "REGISTRATION_ADDED";
    case REGISTRATION_REMOVED:	return xml ? "registration_removed_event" : "REGISTRATION_REMOVED";
    case REGISTRATION_EXPIRED:	return xml ? "registration_expired_event" : "REGISTRATION_EXPIRED";

    case ROUTE_ADD:		return xml ? "route_add_event" : "ROUTE_ADD";
    case ROUTE_DEL:		return xml ? "route_delete_event" : "ROUTE_DEL";
    case ROUTE_QUERY:           return xml ? "route_query" : "ROUTE_QUERY";
    case ROUTE_REPORT:          return xml ? "route_report" : "ROUTE_REPORT";

    case CUSTODY_SIGNAL:	return xml ? "custody_signal_event" : "CUSTODY_SIGNAL";
    case CUSTODY_TIMEOUT:	return xml ? "custody_timeout_event" : "CUSTODY_TIMEOUT";
    
    case DAEMON_SHUTDOWN:	return "SHUTDOWN";
    case DAEMON_STATUS:		return "DAEMON_STATUS";
        
    default:			return "(invalid event type)";
    }
}

/**
 * Possible sources for events.
 */
typedef enum {
    EVENTSRC_PEER   = 1,        ///< a peer dtn forwarder
    EVENTSRC_APP    = 2,	///< a local application
    EVENTSRC_STORE  = 3,	///< the data store
    EVENTSRC_ADMIN  = 4,	///< the admin logic
    EVENTSRC_FRAGMENTATION = 5	///< the fragmentation engine
} event_source_t;

/**
 * Conversion function from a source to a string
 * suitable for use with plug-in arch XML messaging.
 */
inline const char*
source_to_str(event_source_t source)
{
    switch(source) {

    case EVENTSRC_PEER:             return "peer";
    case EVENTSRC_APP:              return "application";
    case EVENTSRC_STORE:            return "dataStore";
    case EVENTSRC_ADMIN:            return "admin";
    case EVENTSRC_FRAGMENTATION:    return "fragmentation";

    default:                        return "(invalid source type)";
    }
}

/**
 * Event base class.
 */
class BundleEvent : public oasys::SerializableObject {
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
     * cleaned up.
     */
    virtual ~BundleEvent() {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}

protected:
    /**
     * Constructor (protected since one of the subclasses should
     * always be that which is actually initialized.
     */
    BundleEvent(event_type_t type)
        : type_(type),
          daemon_only_(false),
          processed_notifier_(NULL) {}
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
                        u_int32_t bytes_received = 0,
                        Contact* originator = NULL)

        : BundleEvent(BUNDLE_RECEIVED),
          bundleref_(bundle, "BundleReceivedEvent"),
          source_(source),
          bytes_received_(bytes_received),
          contact_(originator, "BundleReceivedEvent")
    {
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The newly arrived bundle
    BundleRef bundleref_;

    /// The source of the bundle
    int source_;

    /// The total bytes actually received
    u_int32_t bytes_received_;

    /// Contact from which bundle was received, if applicable
    ContactRef contact_;
};

/**
 * Event class for bundle or fragment transmission.
 */
class BundleTransmittedEvent : public BundleEvent {
public:
    BundleTransmittedEvent(Bundle* bundle, const ContactRef& contact,
                           const LinkRef& link, u_int32_t bytes_sent,
                           u_int32_t reliably_sent)
        : BundleEvent(BUNDLE_TRANSMITTED),
          bundleref_(bundle, "BundleTransmittedEvent"),
          contact_(contact.object(), "BundleTransmittedEvent"),
          bytes_sent_(bytes_sent),
          reliably_sent_(reliably_sent),
          link_(link.object(), "BundleTransmittedEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The transmitted bundle
    BundleRef bundleref_;

    /// The contact where the bundle was sent
    ContactRef contact_;

    /// Total number of bytes sent
    u_int32_t bytes_sent_;

    /// If the convergence layer that we sent on is reliable, this is
    /// the count of the bytes reliably sent, which must be less than
    /// or equal to the bytes transmitted
    u_int32_t reliably_sent_;

    /// The link over which the bundle was sent
    /// (may not have a contact when the transmission result is reported)
    LinkRef link_;

};

/**
 * Event class for a failed transmission, which can occur if a link
 * closes after a router has issued a transmission request but before
 * the bundle is successfully sent.
 */
class BundleTransmitFailedEvent : public BundleEvent {
public:
    BundleTransmitFailedEvent(Bundle* bundle, const ContactRef& contact,
                              const LinkRef& link)
        : BundleEvent(BUNDLE_TRANSMIT_FAILED),
          bundleref_(bundle, "BundleTransmitFailedEvent"),
          contact_(contact.object(), "BundleTransmitFailedEvent"),
          link_(link.object(), "BundleTransmitFailedEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The transmitted bundle
    BundleRef bundleref_;

    /// The contact where the bundle was attempted to be sent
    ContactRef contact_;

    /// The link over which the bundle was sent
    /// (may not have a contact when the transmission result is reported)
    LinkRef link_;
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

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}

    /// The delivered bundle
    BundleRef bundleref_;

    /// The registration that got it
    Registration* registration_;
};

/**
 * Event class for local bundle delivery.
 */
class BundleDeliveryEvent : public BundleEvent {
public:
    BundleDeliveryEvent(Bundle* bundle,
                         event_source_t source)
        : BundleEvent(BUNDLE_DELIVERY),
          bundleref_(bundle, "BundleDeliveryEvent"),
          source_(source) {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The bundle we're delivering
    BundleRef bundleref_;

    /// The source of the bundle
    int source_;
};

/**
 * Event class for bundle expiration.
 */
class BundleExpiredEvent : public BundleEvent {
public:
    BundleExpiredEvent(Bundle* bundle)
        : BundleEvent(BUNDLE_EXPIRED),
          bundleref_(bundle, "BundleExpiredEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

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

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}
    
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
        CL_ERROR,	///< Convergence layer protocol error
        CL_VERSION,	///< Convergence layer version mismatch
        SHUTDOWN,	///< Clean connection shutdown
        RECONNECT,	///< Re-establish link after failure
        IDLE,		///< Idle connection shut down by the CL
        TIMEOUT,	///< Scheduled link ended duration
        BLOCKED, 	///< Link is busy
        UNBLOCKED	///< Link is no longer busy
    } reason_t;

    /**
     * Reason to string conversion.
     */
    static const char* reason_to_str(int reason) {
        switch(reason) {
        case INVALID:	return "INVALID";
        case NO_INFO:	return "no additional info";
        case USER: 	return "user action";
        case SHUTDOWN: 	return "peer shut down";
        case BROKEN:	return "connection broken";
        case CL_ERROR: 	return "cl protocol error";
        case CL_VERSION:return "cl version mismatch";
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

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}

    int reason_;	///< reason code for the event
};

/**
 * Event class for contact up events
 */
class ContactUpEvent : public ContactEvent {
public:
    ContactUpEvent(const ContactRef& contact)
        : ContactEvent(CONTACT_UP),
          contact_(contact.object(), "ContactUpEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

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

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The contact that is now down
    ContactRef contact_;
};

/**
 * Event classes for contact queries and responses
 */
class ContactQueryRequest: public BundleEvent {
public:
    ContactQueryRequest() : BundleEvent(CONTACT_QUERY)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}
};

class ContactReportEvent : public BundleEvent {
public:
    ContactReportEvent() : BundleEvent(CONTACT_REPORT) {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);
};

/**
 * Event class for link creation events
 */
class LinkCreatedEvent : public ContactEvent {
public:
    LinkCreatedEvent(const LinkRef& link)
        : ContactEvent(LINK_CREATED, ContactEvent::USER),
          link_(link.object(), "LinkCreatedEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    LinkRef link_;
};

/**
 * Event class for link deletion events
 */
class LinkDeletedEvent : public ContactEvent {
public:
    LinkDeletedEvent(const LinkRef& link)
        : ContactEvent(LINK_DELETED, ContactEvent::USER),
          link_(link.object(), "LinkDeletedEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The link that is up
    LinkRef link_;
};

/**
 * Event class for link available events
 */
class LinkAvailableEvent : public ContactEvent {
public:
    LinkAvailableEvent(const LinkRef& link, reason_t reason)
        : ContactEvent(LINK_AVAILABLE, reason),
          link_(link.object(), "LinkAvailableEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    LinkRef link_;
};

/**
 * Event class for link unavailable events
 */
class LinkUnavailableEvent : public ContactEvent {
public:
    LinkUnavailableEvent(const LinkRef& link, reason_t reason)
        : ContactEvent(LINK_UNAVAILABLE, reason),
          link_(link.object(), "LinkUnavailableEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The link that is up
    LinkRef link_;
};

/**
 * Event class for link busy events
 */
class LinkBusyEvent : public ContactEvent {
public:
    LinkBusyEvent(const LinkRef& link)
        : ContactEvent(LINK_BUSY, ContactEvent::BLOCKED),
          link_(link.object(), "LinkBusyEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The link that is busy
    LinkRef link_;
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

    LinkStateChangeRequest(const LinkRef& link, state_t state, reason_t reason)
        : ContactEvent(LINK_STATE_CHANGE_REQUEST, reason),
          link_(link.object(), "LinkStateChangeRequest"),
          state_(state), contact_("LinkStateChangeRequest")
    {
        daemon_only_ = true;
        
        contact_   = link->contact();
        old_state_ = link->state();
    }

    LinkStateChangeRequest(const oasys::Builder&,
                           state_t state, reason_t reason)
        : ContactEvent(LINK_STATE_CHANGE_REQUEST, reason),
          state_(state), contact_("LinkStateChangeRequest")
    {
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The link to be changed
    LinkRef link_;

    /// Requested state
    int state_;
    
    /// The active Contact when the request was made
    ContactRef contact_;

    /// State when the request was made
    int old_state_;
};

/**
 * Event class for new registration arrivals.
 */
class RegistrationAddedEvent : public BundleEvent {
public:
    RegistrationAddedEvent(Registration* reg, event_source_t source)
        : BundleEvent(REGISTRATION_ADDED), registration_(reg),
          source_(source) {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*);

    /// The newly added registration
    Registration* registration_;

    /// Why is the registration added
    int source_;
};

/**
 * Event class for registration removals.
 */
class RegistrationRemovedEvent : public BundleEvent {
public:
    RegistrationRemovedEvent(Registration* reg)
        : BundleEvent(REGISTRATION_REMOVED), registration_(reg) {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*);

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

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*);

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

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

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

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The destination eid to be removed
    EndpointIDPattern dest_;
};

/**
 * Event classes for static route queries and responses
 */
class RouteQueryRequest: public BundleEvent {
public:
    RouteQueryRequest() : BundleEvent(ROUTE_QUERY)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}
};

class RouteReportEvent : public BundleEvent {
public:
    RouteReportEvent() : BundleEvent(ROUTE_REPORT) {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}
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

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}

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

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);
    
    /// The parsed data from the custody transfer signal
    CustodySignal::data_t data_;
};

/**
 * Event class for custody transfer timeout events
 */
class CustodyTimeoutEvent : public BundleEvent {
public:
    CustodyTimeoutEvent(Bundle* bundle, const LinkRef& link)
        : BundleEvent(CUSTODY_TIMEOUT),
          bundle_(bundle, "CustodyTimeoutEvent"),
          link_(link.object(), "CustodyTimeoutEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    ///< The bundle whose timer fired
    BundleRef bundle_;

    ///< The link it was sent on
    LinkRef link_;
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

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}
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

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}
};

/**
 * Event class for sending a bundle
 */
class BundleSendRequest: public BundleEvent {
public:
    BundleSendRequest() : BundleEvent(BUNDLE_SEND)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    ///< Bundle ID
    u_int32_t bundleid_;

    ///< Link on which to send the bundle
    std::string link_;

    ///< Forwarding action to use when sending bundle
    int action_;
};

/**
 * Event class for canceling a bundle transmission
 */
class BundleCancelRequest: public BundleEvent {
public:
    BundleCancelRequest() : BundleEvent(BUNDLE_CANCEL)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    ///< Bundle ID
    u_int32_t bundleid_;

    ///< Link where the bundle is destined
    std::string link_;
};

/**
 * Event class for injecting a bundle
 */
class BundleInjectRequest: public BundleEvent {
public:
    BundleInjectRequest() : BundleEvent(BUNDLE_INJECT)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    // Bundle properties
    std::string src_;
    std::string dest_;
    std::string replyto_;
    std::string custodian_;
    u_int8_t    priority_;
    u_int32_t   expiration_;
    std::string payload_;

    // Outgoing link
    std::string link_;

    // Forwarding action
    int action_;
};

/**
 * Event class to optionally probe if a bundle can be accepted by the
 * system before a BundleReceivedEvent is posted. Currently used for
 * backpressure via the API.
 *
 * Note that the bundle may not be completely constructed when this
 * event is posted. In particular, the payload need not be filled in
 * yet, and other security fields may not be present. At a minimum
 * though, the fields from the primary block and the payload length
 * must be known.
 */
class BundleAcceptRequest : public BundleEvent {
public:
    BundleAcceptRequest(const BundleRef& bundle,
                        event_source_t   source,
                        bool*            result,
                        int*             reason)
        : BundleEvent(BUNDLE_ACCEPT_REQUEST),
          bundle_(bundle.object(), "BundleAcceptRequest"),
          source_(source),
          result_(result),
          reason_(reason)
    {
    }
    
    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The newly arrived bundle
    BundleRef bundle_;

    /// The source of the event
    int source_;

    /// Pointer to the expected result
    bool* result_;

    /// Pointer to the reason code
    int* reason_;
};

/**
 * Event classes for bundle queries and responses
 */
class BundleQueryRequest: public BundleEvent {
public:
    BundleQueryRequest() : BundleEvent(BUNDLE_QUERY)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}
};

class BundleReportEvent : public BundleEvent {
public:
    BundleReportEvent() : BundleEvent(BUNDLE_REPORT) {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);
};

/**
 * Event class for creating and opening a link
 */
class LinkCreateRequest: public BundleEvent {
public:
    LinkCreateRequest() : BundleEvent(LINK_CREATE)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {
        NOTIMPLEMENTED;
    }

    ///< Next hop destination
    std::string endpoint_;

    ///< Egress interface
    Interface *interface_;
};

/**
 * Event classes for link queries and responses
 */
class LinkQueryRequest: public BundleEvent {
public:
    LinkQueryRequest() : BundleEvent(LINK_QUERY)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}
};

class LinkReportEvent : public BundleEvent {
public:
    LinkReportEvent() : BundleEvent(LINK_REPORT) {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);
};

} // namespace dtn

#endif /* _BUNDLE_EVENT_H_ */

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
#include "BundleProtocol.h"
#include "BundleRef.h"
#include "BundleList.h"
#include "CustodySignal.h"
#include "contacts/Link.h"
#include "contacts/NamedAttribute.h"
#include "GbofId.h"

#include <oasys/serialize/Serialize.h>
#include <oasys/serialize/SerializeHelpers.h>

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
    BUNDLE_RECEIVED = 0x1,      ///< New bundle arrival
    BUNDLE_TRANSMITTED,         ///< Bundle or fragment successfully sent
    BUNDLE_TRANSMIT_FAILED,     ///< Bundle or fragment successfully sent
    BUNDLE_DELIVERED,           ///< Bundle locally delivered
    BUNDLE_DELIVERY,            ///< Bundle delivery (with payload)
    BUNDLE_EXPIRED,             ///< Bundle expired
    BUNDLE_FREE,                ///< No more references to the bundle
    BUNDLE_FORWARD_TIMEOUT,     ///< A Mapping timed out
    BUNDLE_SEND,                ///< Send a bundle
    BUNDLE_CANCEL,              ///< Cancel a bundle transmission
    BUNDLE_CANCELLED,           ///< Bundle send cancelled
    BUNDLE_INJECT,              ///< Inject a bundle
    BUNDLE_INJECTED,            ///< A bundle was injected
    BUNDLE_ACCEPT_REQUEST,      ///< Request acceptance of a new bundle
    BUNDLE_DELETE,              ///< Request deletion of a bundle
    BUNDLE_QUERY,               ///< Bundle query
    BUNDLE_REPORT,              ///< Response to bundle query
    BUNDLE_ATTRIB_QUERY,        ///< Query for a bundle's attributes
    BUNDLE_ATTRIB_REPORT,       ///< Report with bundle attributes

    CONTACT_UP,                 ///< Contact is up
    CONTACT_DOWN,               ///< Contact abnormally terminated
    CONTACT_QUERY,              ///< Contact query
    CONTACT_REPORT,             ///< Response to contact query
    CONTACT_ATTRIB_CHANGED,     ///< An attribute changed

    LINK_CREATED,               ///< Link is created into the system
    LINK_DELETED,               ///< Link is deleted from the system
    LINK_AVAILABLE,             ///< Link is available
    LINK_UNAVAILABLE,           ///< Link is unavailable
    LINK_BUSY,                  ///< Link is busy 
    LINK_CREATE,                ///< Create and open a new link
    LINK_DELETE,                ///< Delete a link
    LINK_RECONFIGURE,           ///< Reconfigure a link
    LINK_QUERY,                 ///< Link query
    LINK_REPORT,                ///< Response to link query
    LINK_ATTRIB_CHANGED,        ///< An attribute changed

    LINK_STATE_CHANGE_REQUEST,  ///< Link state should be changed

    REASSEMBLY_COMPLETED,       ///< Reassembly completed

    REGISTRATION_ADDED,         ///< New registration arrived
    REGISTRATION_REMOVED,       ///< Registration removed
    REGISTRATION_EXPIRED,       ///< Registration expired

    ROUTE_ADD,                  ///< Add a new entry to the route table
    ROUTE_DEL,                  ///< Remove an entry from the route table
    ROUTE_QUERY,                ///< Static route query
    ROUTE_REPORT,               ///< Response to static route query

    CUSTODY_SIGNAL,             ///< Custody transfer signal received
    CUSTODY_TIMEOUT,            ///< Custody transfer timer fired

    DAEMON_SHUTDOWN,            ///< Shut the daemon down cleanly
    DAEMON_STATUS,              ///< No-op event to check the daemon

    CLA_SET_PARAMS,             ///< Set CLA configuration
    CLA_PARAMS_SET,             ///< CLA configuration changed
    CLA_SET_LINK_DEFAULTS,      ///< Set defaults for new links
    CLA_EID_REACHABLE,          ///< A new EID has been discovered

    CLA_BUNDLE_QUEUED_QUERY,    ///< Query if a bundle is queued at the CLA
    CLA_BUNDLE_QUEUED_REPORT,   ///< Report if a bundle is queued at the CLA
    CLA_EID_REACHABLE_QUERY,    ///< Query if an EID is reachable by the CLA
    CLA_EID_REACHABLE_REPORT,   ///< Report if an EID is reachable by the CLA
    CLA_LINK_ATTRIB_QUERY,      ///< Query CLA for a link's attributes
    CLA_LINK_ATTRIB_REPORT,     ///< Report from CLA with link attributes
    CLA_IFACE_ATTRIB_QUERY,     ///< Query CLA for an interface's attributes
    CLA_IFACE_ATTRIB_REPORT,    ///< Report from CLA with interface attributes
    CLA_PARAMS_QUERY,           ///< Query CLA for config parameters
    CLA_PARAMS_REPORT,          ///< Report from CLA with config paramters

} event_type_t;

/**
 * Conversion function from an event to a string.
 */
inline const char*
event_to_str(event_type_t event, bool xml=false)
{
    switch(event) {

    case BUNDLE_RECEIVED:       return xml ? "bundle_received_event" : "BUNDLE_RECEIVED";
    case BUNDLE_TRANSMITTED:    return xml ? "bundle_transmitted_event" : "BUNDLE_TRANSMITTED";
    case BUNDLE_TRANSMIT_FAILED:return xml ? "bundle_transmit_failed_event" : "BUNDLE_TRANSMIT_FAILED";
    case BUNDLE_DELIVERED:      return xml ? "bundle_delivered_event" : "BUNDLE_DELIVERED";
    case BUNDLE_DELIVERY:       return xml ? "bundle_delivery_event" : "BUNDLE_DELIVERY";
    case BUNDLE_EXPIRED:        return xml ? "bundle_expired_event" : "BUNDLE_EXPIRED";
    case BUNDLE_FREE:           return "BUNDLE_FREE";
    case BUNDLE_FORWARD_TIMEOUT:return "BUNDLE_FORWARD_TIMEOUT";
    case BUNDLE_SEND:           return xml ? "send_bundle_action" : "BUNDLE_SEND";
    case BUNDLE_CANCEL:         return xml ? "cancel_bundle_action" : "BUNDLE_CANCEL";
    case BUNDLE_CANCELLED:      return xml ? "bundle_cancelled_event" : "BUNDLE_CANCELLED";
    case BUNDLE_INJECT:         return xml ? "inject_bundle_action" : "BUNDLE_INJECT";
    case BUNDLE_INJECTED:         return xml ? "bundle_injected_event" : "BUNDLE_INJECTED";
    case BUNDLE_ACCEPT_REQUEST: return xml ? "bundle_accept_request" : "BUNDLE_ACCEPT_REQUEST";
    case BUNDLE_DELETE:         return "BUNDLE_DELETE";
    case BUNDLE_QUERY:          return xml ? "bundle_query" : "BUNDLE_QUERY";
    case BUNDLE_REPORT:         return xml ? "bundle_report" : "BUNDLE_REPORT";

    case CONTACT_UP:            return xml ? "contact_up_event" : "CONTACT_UP";
    case CONTACT_DOWN:          return xml ? "contact_down_event" : "CONTACT_DOWN";
    case CONTACT_QUERY:         return xml ? "contact_query" : "CONTACT_QUERY";
    case CONTACT_REPORT:        return xml ? "contact_report" : "CONTACT_REPORT";
    case CONTACT_ATTRIB_CHANGED:return xml ? "contact_attrib_change_event" : "CONTACT_ATTRIB_CHANGED";

    case LINK_CREATED:          return xml ? "link_created_event" : "LINK_CREATED";
    case LINK_DELETED:          return xml ? "link_deleted_event" : "LINK_DELETED";
    case LINK_AVAILABLE:        return xml ? "link_available_event" : "LINK_AVAILABLE";
    case LINK_UNAVAILABLE:      return xml ? "link_unavailable_event" : "LINK_UNAVAILABLE";
    case LINK_BUSY:             return xml ? "link_busy_event" : "LINK_BUSY";
    case LINK_CREATE:           return "LINK_CREATE";
    case LINK_DELETE:           return "LINK_DELETE";
    case LINK_RECONFIGURE:      return "LINK_RECONFIGURE";
    case LINK_QUERY:            return xml ? "link_query" : "LINK_QUERY";
    case LINK_REPORT:           return xml ? "link_report" : "LINK_REPORT";
    case LINK_ATTRIB_CHANGED:   return xml ? "link_attrib_change_event" : "LINK_ATTRIB_CHANGED";

    case LINK_STATE_CHANGE_REQUEST:return "LINK_STATE_CHANGE_REQUEST";

    case REASSEMBLY_COMPLETED:  return "REASSEMBLY_COMPLETED";

    case REGISTRATION_ADDED:    return xml ? "registration_added_event" : "REGISTRATION_ADDED";
    case REGISTRATION_REMOVED:  return xml ? "registration_removed_event" : "REGISTRATION_REMOVED";
    case REGISTRATION_EXPIRED:  return xml ? "registration_expired_event" : "REGISTRATION_EXPIRED";

    case ROUTE_ADD:             return xml ? "route_add_event" : "ROUTE_ADD";
    case ROUTE_DEL:             return xml ? "route_delete_event" : "ROUTE_DEL";
    case ROUTE_QUERY:           return xml ? "route_query" : "ROUTE_QUERY";
    case ROUTE_REPORT:          return xml ? "route_report" : "ROUTE_REPORT";

    case CUSTODY_SIGNAL:        return xml ? "custody_signal_event" : "CUSTODY_SIGNAL";
    case CUSTODY_TIMEOUT:       return xml ? "custody_timeout_event" : "CUSTODY_TIMEOUT";
    
    case DAEMON_SHUTDOWN:       return "SHUTDOWN";
    case DAEMON_STATUS:         return "DAEMON_STATUS";
        
    case CLA_SET_PARAMS:        return xml ? "cla_set_params_action" : "CLA_SET_PARAMS";
    case CLA_PARAMS_SET:        return xml ? "cla_params_set_event" : "CLA_PARAMS_SET";
    case CLA_SET_LINK_DEFAULTS: return xml ? "cla_set_link_defaults_action" : "CLA_SET_LINK_DEFAULTS";
    case CLA_EID_REACHABLE:     return xml ? "cla_eid_reachable_event" : "CLA_EID_REACHABLE";

    case CLA_BUNDLE_QUEUED_QUERY:
        return xml ? "query_bundle_queued" : "CLA_BUNDLE_QUEUED_QUERY";
    case CLA_BUNDLE_QUEUED_REPORT:
        return xml ? "report_bundle_queued" : "CLA_BUNDLE_QUEUED_REPORT";
    case CLA_EID_REACHABLE_QUERY:
        return xml ? "query_eid_reachable" : "CLA_EID_REACHABLE_QUERY";
    case CLA_EID_REACHABLE_REPORT:
        return xml ? "report_eid_reachable" : "CLA_EID_REACHABLE_REPORT";
    case CLA_LINK_ATTRIB_QUERY:
        return xml ? "query_link_attributes" : "CLA_LINK_ATTRIB_QUERY";
    case CLA_LINK_ATTRIB_REPORT:
        return xml ? "report_link_attributes" : "CLA_LINK_ATTRIB_REPORT";
    case CLA_IFACE_ATTRIB_QUERY:
        return xml ? "query_interface_attributes" : "CLA_IFACE_ATTRIB_QUERY";
    case CLA_IFACE_ATTRIB_REPORT:
        return xml ? "report_interface_attributes" : "CLA_IFACE_ATTRIB_REPORT";
    case CLA_PARAMS_QUERY:
        return xml ? "query_cla_parameters" : "CLA_PARAMS_QUERY";
    case CLA_PARAMS_REPORT:
        return xml ? "report_cla_parameters" : "CLA_PARAMS_REPORT";

    default:                    return "(invalid event type)";
    }
}

/**
 * Possible sources for events.
 */
typedef enum {
    EVENTSRC_PEER   = 1,        ///< a peer dtn forwarder
    EVENTSRC_APP    = 2,        ///< a local application
    EVENTSRC_STORE  = 3,        ///< the data store
    EVENTSRC_ADMIN  = 4,        ///< the admin logic
    EVENTSRC_FRAGMENTATION = 5  ///< the fragmentation engine
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
/*class BundleTransmitFailedEvent : public BundleEvent {
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
};*/

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
        INVALID = 0,    ///< Should not be used
        NO_INFO,        ///< No additional info
        USER,           ///< User action (i.e. console / config)
        BROKEN,         ///< Unexpected session interruption
        DISCOVERY,      ///< Dynamically discovered link
        CL_ERROR,       ///< Convergence layer protocol error
        CL_VERSION,     ///< Convergence layer version mismatch
        SHUTDOWN,       ///< Clean connection shutdown
        RECONNECT,      ///< Re-establish link after failure
        IDLE,           ///< Idle connection shut down by the CL
        TIMEOUT,        ///< Scheduled link ended duration
        BLOCKED,        ///< Link is busy
        UNBLOCKED,      ///< Link is no longer busy
    } reason_t;

    /**
     * Reason to string conversion.
     */
    static const char* reason_to_str(int reason) {
        switch(reason) {
        case INVALID:   return "INVALID";
        case NO_INFO:   return "no additional info";
        case USER:      return "user action";
        case DISCOVERY: return "link discovery";
        case SHUTDOWN:  return "peer shut down";
        case BROKEN:    return "connection broken";
        case CL_ERROR:  return "cl protocol error";
        case CL_VERSION:return "cl version mismatch";
        case RECONNECT: return "re-establishing connection";
        case IDLE:      return "connection idle";
        case TIMEOUT:   return "schedule timed out";
        case UNBLOCKED: return "no longer busy";
        }
        NOTREACHED;
    }

    /// Constructor
    ContactEvent(event_type_t type, reason_t reason = NO_INFO)
        : BundleEvent(type), reason_(reason) {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}

    int reason_;        ///< reason code for the event
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
 * Event class for a change in contact attributes.
 */
class ContactAttributeChangedEvent: public ContactEvent {
public:
    ContactAttributeChangedEvent(const ContactRef& contact, reason_t reason)
        : ContactEvent(CONTACT_ATTRIB_CHANGED, reason),
          contact_(contact.object(), "ContactAttributeChangedEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*);

    ///< The link/contact that changed
    ContactRef contact_;
};

/**
 * Event class for link creation events
 */
class LinkCreatedEvent : public ContactEvent {
public:
    LinkCreatedEvent(const LinkRef& link, reason_t reason = ContactEvent::USER)
        : ContactEvent(LINK_CREATED, reason),
          link_(link.object(), "LinkCreatedEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The link that was created
    LinkRef link_;
};

/**
 * Event class for link deletion events
 */
class LinkDeletedEvent : public ContactEvent {
public:
    LinkDeletedEvent(const LinkRef& link, reason_t reason = ContactEvent::USER)
        : ContactEvent(LINK_DELETED, reason),
          link_(link.object(), "LinkDeletedEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a);

    /// The link that was deleted
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

    /// The link that is available
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

    /// The link that is unavailable
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
 * Event class for shutting down a daemon. The daemon closes and
 * deletes all links, then cleanly closes the various data stores,
 * then calls exit().
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
 
    BundleSendRequest(const BundleRef& bundle,
                      const std::string& link,
                      int action)
        : BundleEvent(BUNDLE_SEND),
          bundle_(bundle.object(), "BundleSendRequest"),
          link_(link),
          action_(action)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
 
    ///< Bundle to be sent
    BundleRef bundle_;

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

    BundleCancelRequest(const BundleRef& bundle, const std::string& link) 
        : BundleEvent(BUNDLE_CANCEL),
          bundle_(bundle.object(), "BundleCancelRequest"),
          link_(link)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    ///< Bundle to be cancelled
    BundleRef bundle_;

    ///< Link where the bundle is destined
    std::string link_;
};

/**
 * Event class for succesful cancellation of a bundle send.
 */
class BundleSendCancelledEvent : public BundleEvent {
public:
    BundleSendCancelledEvent(Bundle* bundle, const LinkRef& link)
        : BundleEvent(BUNDLE_CANCELLED),
          bundleref_(bundle, "BundleSendCancelledEvent"),
          link_(link.object(), "BundleSendCancelledEvent") {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*);

    /// The cancelled bundle
    BundleRef bundleref_;

    /// The link it was queued on
    LinkRef link_;
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
        payload_ = NULL;
    }
    
    ~BundleInjectRequest() 
    {
        delete payload_;
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
    u_char*     payload_;
    size_t      payload_length_;

    // Outgoing link
    std::string link_;

    // Forwarding action
    int action_;

    // Request ID for the event, to identify corresponding BundleInjectedEvent
    std::string request_id_;
};

/**
 * Event class for a succesful bundle injection
 */
class BundleInjectedEvent: public BundleEvent {
public:
    BundleInjectedEvent(Bundle *bundle, const std::string &request_id)
        : BundleEvent(BUNDLE_INJECTED),
          bundleref_(bundle, "BundleInjectedEvent"),
          request_id_(request_id)
    {
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*);

    /// The injected bundle
    BundleRef bundleref_;

    // Request ID from the inject request
    std::string request_id_;
};

/**
 * Event class for requestion deletion of a bundle
 */
class BundleDeleteRequest: public BundleEvent {
public:
    BundleDeleteRequest() : BundleEvent(BUNDLE_DELETE)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    BundleDeleteRequest(const BundleRef&       bundle,
                        BundleProtocol::status_report_reason_t reason)
        : BundleEvent(BUNDLE_DELETE),
          bundle_(bundle.object(), "BundleDeleteRequest"),
          reason_(reason)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    ///< Bundle to be deleted
    BundleRef bundle_;

    /// The reason code
    BundleProtocol::status_report_reason_t reason_;
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

class BundleAttributesQueryRequest: public BundleEvent {
public:
    BundleAttributesQueryRequest(const std::string& query_id,
                                 const BundleRef& bundle,
                                 const AttributeNameVector& attribute_names)
        : BundleEvent(BUNDLE_ATTRIB_QUERY),
          query_id_(query_id),
          bundle_(bundle.object(), "BundleAttributesQueryRequest"),
          attribute_names_(attribute_names) {}

    /// Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    /// Query Identifier
    std::string query_id_;

    ///< Bundle being queried
    BundleRef bundle_;

    /// bundle attributes requested by name.
    AttributeNameVector attribute_names_;

    /// extension blocks requested by type/identifier
    AttributeVector extension_blocks_;
};

class BundleAttributesReportEvent: public BundleEvent {
public:
    BundleAttributesReportEvent(const std::string& query_id,
                                const BundleRef& bundle,
                                const AttributeNameVector& attribute_names,
                                const AttributeVector& extension_blocks)
        : BundleEvent(BUNDLE_ATTRIB_REPORT),
          query_id_(query_id),
          bundle_(bundle.object(), "BundleAttributesReportEvent"),
          attribute_names_(attribute_names),
          extension_blocks_(extension_blocks) {}

    /// Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    /// Query Identifier
    std::string query_id_;

    /// Bundle that was queried
    BundleRef bundle_;

    /// bundle attributes requested by name.
    AttributeNameVector attribute_names_;

    /// extension blocks requested by type/identifier
    AttributeVector extension_blocks_;
};

/**
 * Event class for creating and opening a link
 */
class LinkCreateRequest: public BundleEvent {
public:
    LinkCreateRequest(const std::string &name, Link::link_type_t link_type,
                      const std::string &endpoint,
                      ConvergenceLayer *cla, AttributeVector &parameters)
        : BundleEvent(LINK_CREATE),
          name_(name),
          endpoint_(endpoint),
          link_type_(link_type),
          cla_(cla),
          parameters_(parameters)
        
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {
        NOTIMPLEMENTED;
    }

    ///< Identifier for the link
    std::string name_;

    ///< Next hop EID
    std::string endpoint_;

    ///< Type of link
    Link::link_type_t link_type_;

    ///< CL to use
    ConvergenceLayer *cla_;

    ///< An optional set of key, value pairs
    AttributeVector parameters_;
};

/**
 * Event class for reconfiguring an existing link.
 */
class LinkReconfigureRequest: public BundleEvent {
public:
    LinkReconfigureRequest(const LinkRef& link,
                           AttributeVector &parameters)
        : BundleEvent(LINK_RECONFIGURE),
          link_(link.object(), "LinkReconfigureRequest"),
          parameters_(parameters)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*) {}

    ///< The link to be changed
    LinkRef link_;

    ///< Set of key, value pairs
    AttributeVector parameters_;
};

/**
 * Event class for requesting deletion of a link.
 */
class LinkDeleteRequest: public BundleEvent {
public:
    LinkDeleteRequest(const LinkRef& link) :
        BundleEvent(LINK_DELETE),
        link_(link.object(), "LinkDeleteRequest")
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from Serializable Object
    virtual void serialize(oasys::SerializeAction*) {}

    ///< The link to be deleted
    LinkRef link_;
};

/**
 * Event class for a change in link attributes.
 */
class LinkAttributeChangedEvent: public ContactEvent {
public:
    LinkAttributeChangedEvent(const LinkRef& link,
                              AttributeVector attributes,
                              reason_t reason = ContactEvent::NO_INFO)
        : ContactEvent(LINK_ATTRIB_CHANGED, reason),
          link_(link.object(), "LinkAttributeChangedEvent"),
          attributes_(attributes)
    {
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*);

    ///< The link that changed
    LinkRef link_;

    ///< Attributes that changed
    AttributeVector attributes_;
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

/**
 * Event class for DP-originated CLA parameter change requests.
 */
class CLASetParamsRequest : public BundleEvent {
public:
    CLASetParamsRequest(ConvergenceLayer *cla, AttributeVector &parameters)
        : BundleEvent(CLA_SET_PARAMS), cla_(cla), parameters_(parameters)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    ///< CL to change
    ConvergenceLayer *cla_;

    ///< Set of key, value pairs
    AttributeVector parameters_;
};

/**
 * Event class for CLA parameter change request completion events.
 */
class CLAParamsSetEvent : public BundleEvent {
public:
    CLAParamsSetEvent(ConvergenceLayer *cla, std::string name)
        : BundleEvent(CLA_PARAMS_SET), cla_(cla), name_(name) {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    ///< CL that changed
    ConvergenceLayer *cla_;

    ///< Name of CL (if cla_ is External)
    std::string name_;
};

/**
 * Event class for DP-originated requests to set link defaults.
 */
class SetLinkDefaultsRequest : public BundleEvent {
public:
    SetLinkDefaultsRequest(AttributeVector &parameters)
        : BundleEvent(CLA_SET_LINK_DEFAULTS), parameters_(parameters)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    ///< Set of key, value pairs
    AttributeVector parameters_;
};

/**
 * Event class for discovery of a new EID.
 */
class NewEIDReachableEvent: public BundleEvent {
public:
    NewEIDReachableEvent(Interface* iface, const std::string &endpoint)
        : BundleEvent(CLA_EID_REACHABLE),
          iface_(iface),
          endpoint_(endpoint) {}

    // Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction*);

    ///< The interface the peer was discovered on
    Interface* iface_;

    ///> The EID of the discovered peer
    std::string endpoint_;
};

/**
 *  Event classes for queries to and reports from the CLA.
 */

class CLAQueryReport: public BundleEvent {
public:

    /// Query Identifier
    std::string query_id_;

protected:

    /**
     * Constructor; protected because this class should only be
     * instantiated via a subclass.
     */
    CLAQueryReport(event_type_t type,
                   const std::string& query_id,
                   bool daemon_only = false)
        : BundleEvent(type),
          query_id_(query_id)
    {
        daemon_only_ = daemon_only;
    }
};

class BundleQueuedQueryRequest: public CLAQueryReport {
public:
    BundleQueuedQueryRequest(const std::string& query_id,
                             Bundle* bundle,
                             const LinkRef& link)
        : CLAQueryReport(CLA_BUNDLE_QUEUED_QUERY, query_id, true),
          bundle_(bundle, "BundleQueuedQueryRequest"),
          link_(link.object(), "BundleQueuedQueryRequest") {}

    /// Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    /// Bundle to be checked for queued status.
    BundleRef bundle_;

    /// Link on which to check if the given bundle is queued.
    LinkRef link_;
};

class BundleQueuedReportEvent: public CLAQueryReport {
public:
    BundleQueuedReportEvent(const std::string& query_id,
                            bool is_queued)
        : CLAQueryReport(CLA_BUNDLE_QUEUED_REPORT, query_id),
          is_queued_(is_queued) {}

    /// Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    /// True if the queried bundle was queued on the given link;
    /// otherwise false.
    bool is_queued_;
};

class EIDReachableQueryRequest: public CLAQueryReport {
public:
    EIDReachableQueryRequest(const std::string& query_id,
                             Interface* iface,
                             const std::string& endpoint)
        : CLAQueryReport(CLA_EID_REACHABLE_QUERY, query_id, true),
          iface_(iface),
          endpoint_(endpoint) {}

    /// Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    /// Interface on which to check if the given endpoint is reachable.
    Interface* iface_;

    /// Endpoint to be checked for reachable status.
    std::string endpoint_;
};

class EIDReachableReportEvent: public CLAQueryReport {
public:
    EIDReachableReportEvent(const std::string& query_id,
                            bool is_reachable)
        : CLAQueryReport(CLA_EID_REACHABLE_REPORT, query_id),
          is_reachable_(is_reachable) {}

    /// Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    /// True if the queried endpoint is reachable via the given interface;
    /// otherwise false.
    bool is_reachable_;
};

class LinkAttributesQueryRequest: public CLAQueryReport {
public:
    LinkAttributesQueryRequest(const std::string& query_id,
                               const LinkRef& link,
                               const AttributeNameVector& attribute_names)
        : CLAQueryReport(CLA_LINK_ATTRIB_QUERY, query_id, true),
          link_(link.object(), "LinkAttributesQueryRequest"),
          attribute_names_(attribute_names) {}

    /// Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    /// Link for which the given attributes are requested.
    LinkRef link_;

    /// Link attributes requested by name.
    AttributeNameVector attribute_names_;
};

class LinkAttributesReportEvent: public CLAQueryReport {
public:
    LinkAttributesReportEvent(const std::string& query_id,
                              const AttributeVector& attributes)
        : CLAQueryReport(CLA_LINK_ATTRIB_REPORT, query_id),
          attributes_(attributes) {}

    /// Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    /// Link attribute values given by name.
    AttributeVector attributes_;
};

class IfaceAttributesQueryRequest: public CLAQueryReport {
public:
    IfaceAttributesQueryRequest(const std::string& query_id,
                                Interface* iface,
                                const AttributeNameVector& attribute_names)
        : CLAQueryReport(CLA_IFACE_ATTRIB_QUERY, query_id, true),
          iface_(iface),
          attribute_names_(attribute_names) {}

    /// Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    /// Interface for which the given attributes are requested.
    Interface* iface_;

    /// Link attributes requested by name.
    AttributeNameVector attribute_names_;
};

class IfaceAttributesReportEvent: public CLAQueryReport {
public:
    IfaceAttributesReportEvent(const std::string& query_id,
                               const AttributeVector& attributes)
        : CLAQueryReport(CLA_IFACE_ATTRIB_REPORT, query_id),
          attributes_(attributes) {}

    /// Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    /// Interface attribute values by name.
    AttributeVector attributes_;
};

class CLAParametersQueryRequest: public CLAQueryReport {
public:
    CLAParametersQueryRequest(const std::string& query_id,
                              ConvergenceLayer* cla,
                              const AttributeNameVector& parameter_names)
        : CLAQueryReport(CLA_PARAMS_QUERY, query_id, true),
          cla_(cla),
          parameter_names_(parameter_names) {}

    /// Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    /// Convergence layer for which the given parameters are requested.
    ConvergenceLayer* cla_;

    /// Convergence layer parameters requested by name.
    AttributeNameVector parameter_names_;
};

class CLAParametersReportEvent: public CLAQueryReport {
public:
    CLAParametersReportEvent(const std::string& query_id,
                             const AttributeVector& parameters)
        : CLAQueryReport(CLA_PARAMS_REPORT, query_id),
          parameters_(parameters) {}

    /// Virtual function inherited from SerializableObject
    virtual void serialize(oasys::SerializeAction* a) { (void)a; }

    /// Convergence layer parameter values by name.
    AttributeVector parameters_;
};

} // namespace dtn

#endif /* _BUNDLE_EVENT_H_ */

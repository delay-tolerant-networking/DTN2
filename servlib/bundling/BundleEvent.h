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
    BUNDLE_DELIVERED,           ///< Bundle locally delivered
    BUNDLE_DELIVERY,            ///< Bundle delivery (with payload)
    BUNDLE_EXPIRED,             ///< Bundle expired
    BUNDLE_NOT_NEEDED,          ///< Bundle no longer needed
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
    BUNDLE_ACK,                 ///< Receipt acked by app

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

    PRIVATE,                    ///< A private event occured

    REGISTRATION_ADDED,         ///< New registration arrived
    REGISTRATION_REMOVED,       ///< Registration removed
    REGISTRATION_EXPIRED,       ///< Registration expired
    REGISTRATION_DELETE,	///< Registration to be deleted

    DELIVER_BUNDLE_TO_REG,      ///< Deliver bundle to registation

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
event_to_str(event_type_t event)
{
    switch(event) {

    case BUNDLE_RECEIVED:       return "BUNDLE_RECEIVED";
    case BUNDLE_TRANSMITTED:    return "BUNDLE_TRANSMITTED";
    case BUNDLE_DELIVERED:      return "BUNDLE_DELIVERED";
    case BUNDLE_DELIVERY:       return "BUNDLE_DELIVERY";
    case BUNDLE_EXPIRED:        return "BUNDLE_EXPIRED";
    case BUNDLE_FREE:           return "BUNDLE_FREE";
    case BUNDLE_NOT_NEEDED:     return "BUNDLE_NOT_NEEDED";
    case BUNDLE_FORWARD_TIMEOUT:return "BUNDLE_FORWARD_TIMEOUT";
    case BUNDLE_SEND:           return "BUNDLE_SEND";
    case BUNDLE_CANCEL:         return "BUNDLE_CANCEL";
    case BUNDLE_CANCELLED:      return "BUNDLE_CANCELLED";
    case BUNDLE_INJECT:         return "BUNDLE_INJECT";
    case BUNDLE_INJECTED:       return "BUNDLE_INJECTED";
    case BUNDLE_ACCEPT_REQUEST: return "BUNDLE_ACCEPT_REQUEST";
    case BUNDLE_DELETE:         return "BUNDLE_DELETE";
    case BUNDLE_QUERY:          return "BUNDLE_QUERY";
    case BUNDLE_REPORT:         return "BUNDLE_REPORT";
    case BUNDLE_ATTRIB_QUERY:   return "BUNDLE_ATTRIB_QUERY";
    case BUNDLE_ATTRIB_REPORT:  return "BUNDLE_ATTRIB_REPORT";
    case BUNDLE_ACK:            return "BUNDLE_ACK_BY_APP";

    case CONTACT_UP:            return "CONTACT_UP";
    case CONTACT_DOWN:          return "CONTACT_DOWN";
    case CONTACT_QUERY:         return "CONTACT_QUERY";
    case CONTACT_REPORT:        return "CONTACT_REPORT";
    case CONTACT_ATTRIB_CHANGED:return "CONTACT_ATTRIB_CHANGED";

    case LINK_CREATED:          return "LINK_CREATED";
    case LINK_DELETED:          return "LINK_DELETED";
    case LINK_AVAILABLE:        return "LINK_AVAILABLE";
    case LINK_UNAVAILABLE:      return "LINK_UNAVAILABLE";
    case LINK_BUSY:             return "LINK_BUSY";
    case LINK_CREATE:           return "LINK_CREATE";
    case LINK_DELETE:           return "LINK_DELETE";
    case LINK_RECONFIGURE:      return "LINK_RECONFIGURE";
    case LINK_QUERY:            return "LINK_QUERY";
    case LINK_REPORT:           return "LINK_REPORT";
    case LINK_ATTRIB_CHANGED:   return "LINK_ATTRIB_CHANGED";

    case LINK_STATE_CHANGE_REQUEST:return "LINK_STATE_CHANGE_REQUEST";

    case REASSEMBLY_COMPLETED:  return "REASSEMBLY_COMPLETED";

    case PRIVATE:               return "PRIVATE";

    case REGISTRATION_ADDED:    return "REGISTRATION_ADDED";
    case REGISTRATION_REMOVED:  return "REGISTRATION_REMOVED";
    case REGISTRATION_EXPIRED:  return "REGISTRATION_EXPIRED";
    case REGISTRATION_DELETE:   return "REGISTRATION_DELETE";

    case DELIVER_BUNDLE_TO_REG: return "DELIVER_BUNDLE_TO_REG";

    case ROUTE_ADD:             return "ROUTE_ADD";
    case ROUTE_DEL:             return "ROUTE_DEL";
    case ROUTE_QUERY:           return "ROUTE_QUERY";
    case ROUTE_REPORT:          return "ROUTE_REPORT";

    case CUSTODY_SIGNAL:        return "CUSTODY_SIGNAL";
    case CUSTODY_TIMEOUT:       return "CUSTODY_TIMEOUT";
    
    case DAEMON_SHUTDOWN:       return "SHUTDOWN";
    case DAEMON_STATUS:         return "DAEMON_STATUS";
        
    case CLA_SET_PARAMS:        return "CLA_SET_PARAMS";
    case CLA_PARAMS_SET:        return "CLA_PARAMS_SET";
    case CLA_SET_LINK_DEFAULTS: return "CLA_SET_LINK_DEFAULTS";
    case CLA_EID_REACHABLE:     return "CLA_EID_REACHABLE";

    case CLA_BUNDLE_QUEUED_QUERY:  return "CLA_BUNDLE_QUEUED_QUERY";
    case CLA_BUNDLE_QUEUED_REPORT: return "CLA_BUNDLE_QUEUED_REPORT";
    case CLA_EID_REACHABLE_QUERY:  return "CLA_EID_REACHABLE_QUERY";
    case CLA_EID_REACHABLE_REPORT: return "CLA_EID_REACHABLE_REPORT";
    case CLA_LINK_ATTRIB_QUERY:    return "CLA_LINK_ATTRIB_QUERY";
    case CLA_LINK_ATTRIB_REPORT:   return "CLA_LINK_ATTRIB_REPORT";
    case CLA_IFACE_ATTRIB_QUERY:   return "CLA_IFACE_ATTRIB_QUERY";
    case CLA_IFACE_ATTRIB_REPORT:  return "CLA_IFACE_ATTRIB_REPORT";
    case CLA_PARAMS_QUERY:         return "CLA_PARAMS_QUERY";
    case CLA_PARAMS_REPORT:        return "CLA_PARAMS_REPORT";

    default:                   return "(invalid event type)";
        
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
    EVENTSRC_FRAGMENTATION = 5, ///< the fragmentation engine
    EVENTSRC_ROUTER = 6,        ///< the routing logic
    EVENTSRC_CACHE  = 7         ///< the BPQ cache
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
    case EVENTSRC_ROUTER:           return "router";
    case EVENTSRC_CACHE:            return "cache";

    default:                        return "(invalid source type)";
    }
}

class MetadataBlockRequest {
public:
    enum QueryType { QueryByIdentifier, QueryByType, QueryAll };

    MetadataBlockRequest(QueryType query_type, unsigned int query_value) :
        _query_type(query_type), _query_value(query_value) { }

    int          query_type()  const { return _query_type; }
    unsigned int query_value() const { return _query_value; }

private:
    QueryType    _query_type;
    unsigned int _query_value;
};
typedef std::vector<MetadataBlockRequest> MetaBlockRequestVector;

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
     * Slot to record the time that the event was put into the queue.
     */
    oasys::Time posted_time_;

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
     * Constructor for bundles arriving from a peer, named by the
     * prevhop and optionally marked with the link it arrived on.
     */
    BundleReceivedEvent(Bundle*           bundle,
                        event_source_t    source,
                        u_int32_t         bytes_received,
                        const EndpointID& prevhop,
                        Link*             originator = NULL)

        : BundleEvent(BUNDLE_RECEIVED),
          bundleref_(bundle, "BundleReceivedEvent"),
          source_(source),
          bytes_received_(bytes_received),
          link_(originator, "BundleReceivedEvent"),
          prevhop_(prevhop),
          registration_(NULL)
    {
        ASSERT(source == EVENTSRC_PEER);
    }

    /*
     * Constructor for bundles arriving from a local application
     * identified by the given Registration.
     */
    BundleReceivedEvent(Bundle*        bundle,
                        event_source_t source,
                        Registration*  registration)
        : BundleEvent(BUNDLE_RECEIVED),
          bundleref_(bundle, "BundleReceivedEvent"),
          source_(source),
          bytes_received_(0),
          link_("BundleReceivedEvent"),
          prevhop_(EndpointID::NULL_EID()),
          registration_(registration)
    {
    }

    /*
     * Constructor for other "arriving" bundles, including reloading
     * from storage and generated signals.
     */
    BundleReceivedEvent(Bundle*        bundle,
                        event_source_t source)
        : BundleEvent(BUNDLE_RECEIVED),
          bundleref_(bundle, "BundleReceivedEvent"),
          source_(source),
          bytes_received_(0),
          link_("BundleReceivedEvent"),
          prevhop_(EndpointID::NULL_EID()),
          registration_(NULL)
    {
    }

    /// The newly arrived bundle
    BundleRef bundleref_;

    /// The source of the bundle
    int source_;

    /// The total bytes actually received
    u_int32_t bytes_received_;

    /// Link from which bundle was received, if applicable
    LinkRef link_;

    /// Previous hop endpoint id
    EndpointID prevhop_;

    /// Registration where the bundle arrived
    Registration* registration_;
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
 * Event class for acknowledgement of bundle reciept by app.
 */
class BundleAckEvent : public BundleEvent {
public:
    BundleAckEvent(u_int reg, std::string source, u_quad_t secs, u_quad_t seq)
        : BundleEvent(BUNDLE_ACK),
          regid_(reg),
          sourceEID_(source),
          creation_ts_(secs, seq) {}

    u_int regid_;
    EndpointID sourceEID_;
    BundleTimestamp creation_ts_;
};

/**
 * Event class for local bundle delivery.
 * Processing of this event causes the bundle to be delivered
 * to the particular registration (application).
 */
class DeliverBundleToRegEvent : public BundleEvent {
public:
    //DeliverBundleToRegEvent(Bundle* bundle,
    //                        u_int32_t regid,
    //                        Registration* registration)
    //    : BundleEvent(DELIVER_BUNDLE_TO_REG),
    //      bundleref_(bundle, "BundleToRegEvent"),
    //      regid_(regid),
    //      registration_(registration) { }

    DeliverBundleToRegEvent(Bundle* bundle,
                            u_int32_t regid)
        : BundleEvent(DELIVER_BUNDLE_TO_REG),
          bundleref_(bundle, "BundleToRegEvent"),
          regid_(regid) { }

    /// The bundle to be delivered
    BundleRef bundleref_;

    u_int32_t regid_;
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
        }
        NOTREACHED;
    }

    /// Constructor
    ContactEvent(event_type_t type, reason_t reason = NO_INFO)
        : BundleEvent(type), reason_(reason) {}

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
 * Event classes for contact queries and responses
 */
class ContactQueryRequest: public BundleEvent {
public:
    ContactQueryRequest() : BundleEvent(CONTACT_QUERY)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
};

class ContactReportEvent : public BundleEvent {
public:
    ContactReportEvent() : BundleEvent(CONTACT_REPORT) {}
};

/**
 * Event class for a change in contact attributes.
 */
class ContactAttributeChangedEvent: public ContactEvent {
public:
    ContactAttributeChangedEvent(const ContactRef& contact, reason_t reason)
        : ContactEvent(CONTACT_ATTRIB_CHANGED, reason),
          contact_(contact.object(), "ContactAttributeChangedEvent") {}

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

    /// The link that is unavailable
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

    /// The link to be changed
    LinkRef link_;

    /// Requested state
    int state_;
    
    /// The active Contact when the request was made
    ContactRef contact_;

    /// State when the request was made
    int old_state_;
};


/*
 * Private Event used for components to post an event for themselves
 * which will be processed synchronously in the BundleDaemon Event Queue
 */
class PrivateEvent : public BundleEvent {
 public:
  PrivateEvent(int sub_type, void* context) 
    :  BundleEvent(PRIVATE), sub_type_(sub_type), context_(context) {}

  int   sub_type_;
  void* context_;
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
    int source_;
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
    RegistrationExpiredEvent(Registration* registration)
        : BundleEvent(REGISTRATION_EXPIRED),
          registration_(registration) {}
    
    /// The to-be-removed registration 
    Registration* registration_;
};

/**
 * Daemon-only event class used to delete a registration after it's
 * removed or expired.
 */
class RegistrationDeleteRequest : public BundleEvent {
public:
    RegistrationDeleteRequest(Registration* registration)
        : BundleEvent(REGISTRATION_DELETE),
          registration_(registration)
    {
        daemon_only_ = true;
    }

    /// The registration to be deleted
    Registration* registration_;
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
 * Event classes for static route queries and responses
 */
class RouteQueryRequest: public BundleEvent {
public:
    RouteQueryRequest() : BundleEvent(ROUTE_QUERY)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }
};

class RouteReportEvent : public BundleEvent {
public:
    RouteReportEvent() : BundleEvent(ROUTE_REPORT) {}
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
    CustodyTimeoutEvent(Bundle* bundle, const LinkRef& link)
        : BundleEvent(CUSTODY_TIMEOUT),
          bundle_(bundle, "CustodyTimeoutEvent"),
          link_(link.object(), "CustodyTimeoutEvent") {}

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
        daemon_only_ = false;
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
    }
    
    // Bundle properties
    std::string src_;
    std::string dest_;
    std::string replyto_;
    std::string custodian_;
    u_int8_t    priority_;
    u_int32_t   expiration_;
    std::string payload_file_;

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

    BundleDeleteRequest(Bundle* bundle,
                        BundleProtocol::status_report_reason_t reason)
        : BundleEvent(BUNDLE_DELETE),
          bundle_(bundle, "BundleDeleteRequest"),
          reason_(reason)
    {
        // should be processed only by the daemon
        daemon_only_ = true;
    }

    BundleDeleteRequest(const BundleRef& bundle,
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
};

class BundleReportEvent : public BundleEvent {
public:
    BundleReportEvent() : BundleEvent(BUNDLE_REPORT) {}
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

    /// Query Identifier
    std::string query_id_;

    ///< Bundle being queried
    BundleRef bundle_;

    /// bundle attributes requested by name.
    AttributeNameVector attribute_names_;

    /// metadata blocks requested by type/identifier
    MetaBlockRequestVector metadata_blocks_;
};

class BundleAttributesReportEvent: public BundleEvent {
public:
    BundleAttributesReportEvent(const std::string& query_id,
                                const BundleRef& bundle,
                                const AttributeNameVector& attribute_names,
                                const MetaBlockRequestVector& metadata_blocks)
        : BundleEvent(BUNDLE_ATTRIB_REPORT),
          query_id_(query_id),
          bundle_(bundle.object(), "BundleAttributesReportEvent"),
          attribute_names_(attribute_names),
          metadata_blocks_(metadata_blocks) {}

    /// Query Identifier
    std::string query_id_;

    /// Bundle that was queried
    BundleRef bundle_;

    /// bundle attributes requested by name.
    AttributeNameVector attribute_names_;

    /// metadata blocks requested by type/identifier
    MetaBlockRequestVector metadata_blocks_;
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
};

class LinkReportEvent : public BundleEvent {
public:
    LinkReportEvent() : BundleEvent(LINK_REPORT) {}
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

    /// Convergence layer parameter values by name.
    AttributeVector parameters_;
};

} // namespace dtn

#endif /* _BUNDLE_EVENT_H_ */

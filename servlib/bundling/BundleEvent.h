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
#include "Link.h"

namespace dtn {

/**
 * All signaling from various components to the routing layer is done
 * via the Bundle Event message abstraction. This file defines the
 * event type codes and corresponding classes.
 */

class Bundle;
class BundleConsumer;
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
    case BUNDLE_EXPIRED:	return "BUNDLE_EXPIRED";
    case BUNDLE_FREE:		return "BUNDLE_FREE";
    case BUNDLE_FORWARD_TIMEOUT: return "BUNDLE_FORWARD_TIMEOUT";

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
    BundleEvent(event_type_t type) : type_(type), daemon_only_(false) {};
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
    BundleTransmittedEvent(Bundle* bundle, BundleConsumer* consumer,
                           size_t bytes_sent, bool acked)
        : BundleEvent(BUNDLE_TRANSMITTED),
          bundleref_(bundle, "BundleTransmittedEvent"),
          consumer_(consumer),
          bytes_sent_(bytes_sent), acked_(acked) {}

    /// The transmitted bundle
    BundleRef bundleref_;

    /// The contact or registration where the bundle was sent
    BundleConsumer* consumer_;

    /// Total number of bytes sent
    size_t bytes_sent_;

    /// Indication if the destination acknowledged bundle receipt
    bool acked_;

    /// XXX/demmer should have bytes_acked
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
 * Event class for contact up events
 */
class ContactUpEvent : public BundleEvent {
public:
    ContactUpEvent(Contact* contact)
        : BundleEvent(CONTACT_UP), contact_(contact) {}

    /// The contact that is up
    Contact* contact_;
};

/**
 * Event class for contact down events
 */
class ContactDownEvent : public BundleEvent {
public:
    /**
     * Reason codes for why the contact went down.
     */
    typedef enum {
        INVALID = 0,	///< Should not be used
        BROKEN,		///< Unexpected session interruption
        IDLE,		///< Idle connection shut down by the CL
        TIMEOUT		///< Scheduled link ended duration
    } reason_t;

    /**
     * Reason to string conversion.
     */
    static const char* reason_to_str(reason_t reason) {
        switch(reason) {
        case INVALID: return "INVALID";
        case BROKEN:  return "BROKEN";
        case IDLE:    return "IDLE";
        case TIMEOUT: return "TIMEOUT";
        }
        NOTREACHED;
    }
        
    ContactDownEvent(Contact* contact, reason_t reason)
        : BundleEvent(CONTACT_DOWN), contact_(contact), reason_(reason) {}

    /// The contact that is now down
    Contact* contact_;

    /// Why the contact went down
    reason_t reason_;
};

/**
 * Event class for link creation events
 */
class LinkCreatedEvent : public BundleEvent {
public:
    LinkCreatedEvent(Link* link)
        : BundleEvent(LINK_CREATED), link_(link) {}

    Link* link_;
};

/**
 * Event class for link deletion events
 */
class LinkDeletedEvent : public BundleEvent {
public:
    LinkDeletedEvent(Link* link)
        : BundleEvent(LINK_DELETED), link_(link) {}

    /// The link that is up
    Link* link_;
};

/**
 * Event class for link available events
 */
class LinkAvailableEvent : public BundleEvent {
public:
    LinkAvailableEvent(Link* link)
        : BundleEvent(LINK_AVAILABLE), link_(link) {}

    Link* link_;
};

/**
 * Event class for link unavailable events
 */
class LinkUnavailableEvent : public BundleEvent {
public:
    LinkUnavailableEvent(Link* link)
        : BundleEvent(LINK_UNAVAILABLE), link_(link) {}

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
class LinkStateChangeRequest : public BundleEvent {
public:
    /// Shared type code for state_t with Link
    typedef Link::state_t state_t;

    /// Shared type code for reason with ContactDownEvent
    typedef ContactDownEvent::reason_t reason_t;

    /// Constructor used for any operation except closing the link
    LinkStateChangeRequest(Link* link, state_t state)
        : BundleEvent(LINK_STATE_CHANGE_REQUEST),
          link_(link), state_(state), reason_(ContactDownEvent::INVALID)
    {
        daemon_only_ = true;	// should be processed only by the daemon
        
        if (state == Link::CLOSING) {
            PANIC("must specify a reason when closing a link");
        }
    }

    /// Constructor used when closing a link
    LinkStateChangeRequest(Link* link, state_t state, reason_t reason)
        : BundleEvent(LINK_STATE_CHANGE_REQUEST),
          link_(link), state_(state), reason_(reason)
    {
        daemon_only_ = true;
        
        if (state != Link::CLOSING) {
            PANIC("must not specify a reason when not closing a link");
        }
    }

    /// The link to be changed
    Link* link_;

    /// Requested state
    state_t state_;

    /// Reason for closing the link
    reason_t reason_;
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

} // namespace dtn

#endif /* _BUNDLE_EVENT_H_ */

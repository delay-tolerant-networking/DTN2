/*
 *    Copyright 2005-2006 Intel Corporation
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


#include "BundleEventHandler.h"

namespace dtn {

/** 
 * Dispatch the event by type code to one of the event-specific
 * handler functions below.
 */
void
BundleEventHandler::dispatch_event(BundleEvent* e)
{
    log_debug("dispatching event (%p) %s", e, e->type_str());
    
    switch(e->type_) {

    case BUNDLE_RECEIVED:
        handle_bundle_received((BundleReceivedEvent*)e);
        break;

    case BUNDLE_TRANSMITTED:
        handle_bundle_transmitted((BundleTransmittedEvent*)e);
        break;

    case BUNDLE_TRANSMIT_FAILED:
        handle_bundle_transmit_failed((BundleTransmitFailedEvent*)e);
        break;

    case BUNDLE_DELIVERED:
        handle_bundle_delivered((BundleDeliveredEvent*)e);
        break;

    case BUNDLE_EXPIRED:
        handle_bundle_expired((BundleExpiredEvent*)e);
        break;
        
    case BUNDLE_FREE:
        handle_bundle_free((BundleFreeEvent*)e);
        break;

    case BUNDLE_SEND:
        handle_bundle_send((BundleSendRequest*)e);
        break;

    case BUNDLE_CANCEL:
        handle_bundle_cancel((BundleCancelRequest*)e);
        break;

    case BUNDLE_INJECT:
        handle_bundle_inject((BundleInjectRequest*)e);
        break;

    case BUNDLE_ACCEPT_REQUEST:
        handle_bundle_accept((BundleAcceptRequest*)e);
        break;

    case BUNDLE_QUERY:
        handle_bundle_query((BundleQueryRequest*)e);
        break;

    case BUNDLE_REPORT:
        handle_bundle_report((BundleReportEvent*)e);
        break;
        
    case REGISTRATION_ADDED:
        handle_registration_added((RegistrationAddedEvent*)e);
        break;

    case REGISTRATION_REMOVED:
        handle_registration_removed((RegistrationRemovedEvent*)e);
        break;

    case REGISTRATION_EXPIRED:
        handle_registration_expired((RegistrationExpiredEvent*)e);
        break;

    case ROUTE_ADD:
        handle_route_add((RouteAddEvent*)e);
        break;

    case ROUTE_DEL:
        handle_route_del((RouteDelEvent*)e);
        break;

    case ROUTE_QUERY:
        handle_route_query((RouteQueryRequest*)e);
        break;

    case ROUTE_REPORT:
        handle_route_report((RouteReportEvent*)e);
        break;

    case CONTACT_UP:
        handle_contact_up((ContactUpEvent*)e);
        break;

    case CONTACT_DOWN:
        handle_contact_down((ContactDownEvent*)e);
        break;

    case CONTACT_QUERY:
        handle_contact_query((ContactQueryRequest*)e);
        break;

    case CONTACT_REPORT:
        handle_contact_report((ContactReportEvent*)e);
        break;

    case LINK_CREATED:
        handle_link_created((LinkCreatedEvent*)e);
        break;

    case LINK_DELETED:
        handle_link_deleted((LinkDeletedEvent*)e);
        break;

    case LINK_AVAILABLE:
        handle_link_available((LinkAvailableEvent*)e);
        break;

    case LINK_UNAVAILABLE:
        handle_link_unavailable((LinkUnavailableEvent*)e);
        break;

    case LINK_STATE_CHANGE_REQUEST:
        handle_link_state_change_request((LinkStateChangeRequest*)e);
        break;

    case LINK_CREATE:
        handle_link_create((LinkCreateRequest*)e);
        break;

    case LINK_QUERY:
        handle_link_query((LinkQueryRequest*)e);
        break;

    case LINK_REPORT:
        handle_link_report((LinkReportEvent*)e);
        break;
        
    case REASSEMBLY_COMPLETED:
        handle_reassembly_completed((ReassemblyCompletedEvent*)e);
        break;

    case CUSTODY_SIGNAL:
        handle_custody_signal((CustodySignalEvent*)e);
        break;

    case CUSTODY_TIMEOUT:
        handle_custody_timeout((CustodyTimeoutEvent*)e);
        break;

    case DAEMON_SHUTDOWN:
        handle_shutdown_request((ShutdownRequest*)e);
        break;

    case DAEMON_STATUS:
        handle_status_request((StatusRequest*)e);
        break;

    default:
        PANIC("unimplemented event type %d", e->type_);
    }
}

/**
 * Default event handler for new bundle arrivals.
 */
void
BundleEventHandler::handle_bundle_received(BundleReceivedEvent*)
{
}
    
/**
 * Default event handler when bundles are transmitted.
 */
void
BundleEventHandler::handle_bundle_transmitted(BundleTransmittedEvent*)
{
}

/**
 * Default event handler when a bundle transmission fails.
 */
void
BundleEventHandler::handle_bundle_transmit_failed(BundleTransmitFailedEvent*)
{
}

/**
 * Default event handler when bundles are locally delivered.
 */
void
BundleEventHandler::handle_bundle_delivered(BundleDeliveredEvent*)
{
}

/**
 * Default event handler when bundles expire.
 */
void
BundleEventHandler::handle_bundle_expired(BundleExpiredEvent*)
{
}

/**
 * Default event handler when bundles are free (i.e. no more
 * references).
 */
void
BundleEventHandler::handle_bundle_free(BundleFreeEvent*)
{
}

/**
 * Default event handler for bundle send requests
 */
void
BundleEventHandler::handle_bundle_send(BundleSendRequest*)
{
}

/**
 * Default event handler for send bundle request cancellations
 */
void
BundleEventHandler::handle_bundle_cancel(BundleCancelRequest*)
{
}

/**
 * Default event handler for bundle inject requests
 */
void
BundleEventHandler::handle_bundle_inject(BundleInjectRequest*)
{
}

/**
 * Default event handler for a bundle accept request probe.
 */
void
BundleEventHandler::handle_bundle_accept(BundleAcceptRequest*)
{
}

/**
 * Default event handler for bundle query requests.
 */
void
BundleEventHandler::handle_bundle_query(BundleQueryRequest*)
{
}

/**
 * Default event handler for bundle reports.
 */
void
BundleEventHandler::handle_bundle_report(BundleReportEvent*)
{
}

/**
 * Default event handler when a new application registration
 * arrives.
 */
void
BundleEventHandler::handle_registration_added(RegistrationAddedEvent*)
{
}

/**
 * Default event handler when a registration is removed.
 */
void
BundleEventHandler::handle_registration_removed(RegistrationRemovedEvent*)
{
}

/**
 * Default event handler when a registration expires.
 */
void
BundleEventHandler::handle_registration_expired(RegistrationExpiredEvent*)
{
}

/**
 * Default event handler when a new contact is up.
 */
void
BundleEventHandler::handle_contact_up(ContactUpEvent*)
{
}
    
/**
 * Default event handler when a contact is down.
 */
void
BundleEventHandler::handle_contact_down(ContactDownEvent*)
{
}

/**
 * Default event handler for contact query requests.
 */
void
BundleEventHandler::handle_contact_query(ContactQueryRequest*)
{
}

/**
 * Default event handler for contact reports.
 */
void
BundleEventHandler::handle_contact_report(ContactReportEvent*)
{
}

/**
 * Default event handler when a new link is created.
 */
void
BundleEventHandler::handle_link_created(LinkCreatedEvent*)
{
}
    
/**
 * Default event handler when a link is deleted.
 */
void
BundleEventHandler::handle_link_deleted(LinkDeletedEvent*)
{
}

/**
 * Default event handler when link becomes available
 */
void
BundleEventHandler::handle_link_available(LinkAvailableEvent*)
{
}

/**
 * Default event handler when a link is unavailable
 */
void
BundleEventHandler::handle_link_unavailable(LinkUnavailableEvent*)
{
}

/**
 * Default event handler for link state change requests.
 */
void
BundleEventHandler::handle_link_state_change_request(LinkStateChangeRequest*)
{
}

/**
 * Default event handler for link create requests.
 */
void
BundleEventHandler::handle_link_create(LinkCreateRequest*)
{
}

/**
 * Default event handler for link query requests.
 */
void
BundleEventHandler::handle_link_query(LinkQueryRequest*)
{
}

/**
 * Default event handler for link reports.
 */
void
BundleEventHandler::handle_link_report(LinkReportEvent*)
{
}

/**
 * Default event handler when reassembly is completed.
 */
void
BundleEventHandler::handle_reassembly_completed(ReassemblyCompletedEvent*)
{
}
    
/**
 * Default event handler when a new route is added by the command
 * or management interface.
 */
void
BundleEventHandler::handle_route_add(RouteAddEvent*)
{
}

/**
 * Default event handler when a route is deleted by the command
 * or management interface.
 */
void
BundleEventHandler::handle_route_del(RouteDelEvent*)
{
}

/**
 * Default event handler for static route query requests.
 */
void
BundleEventHandler::handle_route_query(RouteQueryRequest*)
{
}

/**
 * Default event handler for static route reports.
 */
void
BundleEventHandler::handle_route_report(RouteReportEvent*)
{
}

/**
 * Default event handler when custody signals are received.
 */
void
BundleEventHandler::handle_custody_signal(CustodySignalEvent*)
{
}
    
/**
 * Default event handler when custody transfer timers expire
 */
void
BundleEventHandler::handle_custody_timeout(CustodyTimeoutEvent*)
{
}
    
/**
 * Default event handler for shutdown requests.
 */
void
BundleEventHandler::handle_shutdown_request(ShutdownRequest*)
{
}

/**
 * Default event handler for status requests.
 */
void
BundleEventHandler::handle_status_request(StatusRequest*)
{
}

} // namespace dtn

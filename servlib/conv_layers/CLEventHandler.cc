/*
Copyright 2004-2006 BBN Technologies Corporation

Licensed under the Apache License, Version 2.0 (the "License"); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at http://www.apache.org/licenses/LICENSE-2.0
 
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.
    
See the License for the specific language governing permissions and limitations
under the License.

$Id$
*/

#include <config.h>
#ifdef XERCES_C_ENABLED

#include <oasys/serialize/XMLSerialize.h>

#include "CLEventHandler.h"


namespace dtn {

void
CLEventHandler::dispatch_cl_event(CLEvent* event)
{
    log_debug_p("/dtn/cl", "Dispatching %s", event->event_name().c_str());

    switch ( event->type() ) {
    case CL_INTERFACE_CREATE:
        handle_cl_interface_create( (CLInterfaceCreateRequest*)event );
        break;

    case CL_INTERFACE_CREATED:
        handle_cl_interface_created( (CLInterfaceCreatedEvent*)event );
        break;

    case CL_LINK_CREATE:
        handle_cl_link_create( (CLLinkCreateRequest*)event );
        break;

    case CL_LINK_CREATED:
        handle_cl_link_created( (CLLinkCreatedEvent*)event );
        break;
        
    case CL_LINK_STATE_CHANGED:
        handle_cl_link_state_changed( (CLLinkStateChangedEvent*)event );
        break;
        
    case CL_UP:
        handle_cl_up( (CLUpEvent*)event );
        break;

    case CL_UP_REPLY:
        handle_cl_up_reply( (CLUpReplyEvent*)event );
        break;

    case CL_BUNDLE_SEND:
        handle_cl_bundle_send( (CLBundleSendRequest*)event );
        break;

    case CL_BUNDLE_TRANSMITTED:
        handle_cl_bundle_transmitted( (CLBundleTransmittedEvent*)event );
        break;
        
    case CL_BUNDLE_TRANSMIT_FAILED:
        handle_cl_bundle_transmit_failed( (CLBundleTransmitFailedEvent*)event );
        break;

    case CL_BUNDLE_RECEIVED:
        handle_cl_bundle_received( (CLBundleReceivedEvent*)event );
        break;
        
    case CL_BUNDLE_HEADER_RECEIVED:
        handle_cl_bundle_header_received( (CLBundleHeaderReceivedEvent*)event );
        break;

    case CL_BUNDLE_PAYLOAD_RECEIVED:
        handle_cl_bundle_payload_received( (CLBundlePayloadReceivedEvent*)event );
        break;

    default:
        break;

    } // switch
}

CLEvent*
CLEventHandler::process_cl_event(const char* message,
                                 oasys::XMLUnmarshal& parser)
{
    CLEvent* event;

    // clear any error condition before next parse
    parser.reset_error();

    // parse the xml payload received
    const char* event_tag = parser.parse(message);

    // was the message valid?
    if (parser.error() || !event_tag)
        return NULL;

    // traverse the XML document, instantiating events
    do {
        event = CLEvent::instantiate(event_tag);
        if (!event)
            continue;

        // unmarshal the action
        parser.action(event);
        break;
    } while ( (event_tag = parser.parse(NULL)) != 0 );

    return event;
}

void
CLEventHandler::clear_parser(oasys::XMLUnmarshal& parser)
{
    const char* event_tag;

    while ( ( event_tag = parser.parse(NULL) ) )
        delete event_tag;
}

} // namespace dtn

#endif // XERCES_C_ENABLED

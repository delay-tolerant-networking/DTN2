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

#ifndef _CL_EVENT_HANDLER_H_
#define _CL_EVENT_HANDLER_H_

#include <config.h>
#ifdef XERCES_C_ENABLED

#include <oasys/util/StringBuffer.h>
#include <oasys/serialize/XMLSerialize.h>

#include "CLEvent.h"

namespace dtn {

using namespace dtn::clevent;

class CLEventHandler : public oasys::Logger {
protected:
    CLEventHandler(const char *classname, const std::string &logpath)
        : oasys::Logger(classname, logpath) { }
    virtual ~CLEventHandler() { }
    CLEvent* process_cl_event(const char* message, oasys::XMLUnmarshal& parser);
    void dispatch_cl_event(CLEvent* event);
    void clear_parser(oasys::XMLUnmarshal& parser);

    virtual void handle_cl_up(CLUpEvent* event) { (void)event; }
    virtual void handle_cl_up_reply(CLUpReplyEvent* event) { (void)event; }
    virtual void handle_cl_interface_create(CLInterfaceCreateRequest* event) { (void)event; }
    virtual void handle_cl_interface_created(CLInterfaceCreatedEvent* event) { (void)event; }
    virtual void handle_cl_link_create(CLLinkCreateRequest* event) { (void)event; }
    virtual void handle_cl_link_created(CLLinkCreatedEvent* event) { (void)event; }
    virtual void handle_cl_link_state_changed(CLLinkStateChangedEvent* event) { (void)event; }
    virtual void handle_cl_bundle_send(CLBundleSendRequest* event) { (void)event; }
    virtual void handle_cl_bundle_transmitted(CLBundleTransmittedEvent* event) { (void)event; }
    virtual void handle_cl_bundle_transmit_failed(CLBundleTransmitFailedEvent* event) { (void)event; }
    virtual void handle_cl_bundle_received(CLBundleReceivedEvent* event) { (void)event; }
    virtual void handle_cl_bundle_header_received(CLBundleHeaderReceivedEvent* event) { (void)event; }
    virtual void handle_cl_bundle_payload_received(CLBundlePayloadReceivedEvent* event) { (void)event; }
    
}; // class CLEventHandler

} // namespace dtn

#endif // XERCES_C_ENABLED
#endif

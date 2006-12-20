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

#include "CLEvent.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "contacts/Link.h"

namespace dtn {

namespace clevent {

static void process_link_type(const char* name, Link::link_type_t* value, 
                              oasys::SerializeAction* a) 
{
    std::string value_string;
    
    if (a->action_code() == oasys::SerializeAction::UNMARSHAL) {
        a->process(name, &value_string);
        
        if ( value_string == std::string("alwayson") )
            *value = Link::ALWAYSON;
        else if ( value_string == std::string("ondemand") )
            *value = Link::ONDEMAND;
        else if ( value_string == std::string("scheduled") )
            *value = Link::SCHEDULED;
        else if ( value_string == std::string("opportunistic") )
            *value = Link::OPPORTUNISTIC;
    }
            
    else if (a->action_code() == oasys::SerializeAction::MARSHAL) {
        switch (*value) {            
            case Link::ALWAYSON:
                value_string = std::string("alwayson");
                break;
            case Link::ONDEMAND:
                value_string = std::string("ondemand");
                break;
            case Link::SCHEDULED: 
                value_string = std::string("scheduled"); 
                break;
            case Link::OPPORTUNISTIC:
                value_string = std::string("opportunistic"); 
                break;
            default:
                value_string = std::string("invalid");
                break;
        }
        
        a->process(name, &value_string);
    }
}


static void process_link_state(const char* name, Link::state_t* value, 
                               oasys::SerializeAction* a) 
{
    std::string value_string;
    
    if (a->action_code() == oasys::SerializeAction::UNMARSHAL) {
        a->process(name, &value_string);
        
        if ( value_string == std::string("unavailable") )
            *value = Link::UNAVAILABLE;
        else if ( value_string == std::string("available") )
            *value = Link::AVAILABLE;
        else if ( value_string == std::string("opening") )
            *value = Link::OPENING;
        else if ( value_string == std::string("open") )
            *value = Link::OPEN;
        else if ( value_string == std::string("busy") )
            *value = Link::BUSY;
        else if ( value_string == std::string("closed") )
            *value = Link::CLOSED;
    }
            
    else if (a->action_code() == oasys::SerializeAction::MARSHAL) {
        switch (*value) {            
            case Link::UNAVAILABLE:
                value_string = std::string("unavailable");
                break;
            case Link::AVAILABLE:
                value_string = std::string("available");
                break;
            case Link::OPENING: 
                value_string = std::string("opening"); 
                break;
            case Link::OPEN: 
                value_string = std::string("open"); 
                break;
            case Link::BUSY: 
                value_string = std::string("busy"); 
                break;
            case Link::CLOSED: 
                value_string = std::string("closed"); 
                break;
            default:
                return;
        }
        
        a->process(name, &value_string);
    }
}


static void process_link_state_reason(const char* name,
                    ContactEvent::reason_t* value, oasys::SerializeAction* a)
{
    std::string value_string;
    
    if (a->action_code() == oasys::SerializeAction::UNMARSHAL) {
        a->process(name, &value_string);
        
        if ( value_string == std::string("no_info") )
            *value = ContactEvent::NO_INFO;
        else if ( value_string == std::string("user") )
            *value = ContactEvent::USER;
        else if ( value_string == std::string("broken") )
            *value = ContactEvent::BROKEN;
        else if ( value_string == std::string("shutdown") )
            *value = ContactEvent::SHUTDOWN;
        else if ( value_string == std::string("reconnect") )
            *value = ContactEvent::RECONNECT;
        else if ( value_string == std::string("idle") )
            *value = ContactEvent::IDLE;
        else if ( value_string == std::string("timeout") )
            *value = ContactEvent::TIMEOUT;
        else if ( value_string == std::string("unblocked") )
            *value = ContactEvent::UNBLOCKED;
    }

    else if (a->action_code() == oasys::SerializeAction::MARSHAL) {
        switch (*value) {
            case ContactEvent::NO_INFO:
                value_string = std::string("no_info");
                break;
            case ContactEvent::USER:
                value_string = std::string("user");
                break;
            case ContactEvent::BROKEN:
                value_string = std::string("broken");
                break;
            case ContactEvent::SHUTDOWN:
                value_string = std::string("shutdown");
                break;
            case ContactEvent::RECONNECT:
                value_string = std::string("reconnect");
                break;
            case ContactEvent::IDLE:
                value_string = std::string("idle");
                break;
            case ContactEvent::TIMEOUT:
                value_string = std::string("timeout");
                break;
            case ContactEvent::UNBLOCKED:
                value_string = std::string("unblocked");
                break;
            default:
                return;
        }
        
        a->process(name, &value_string);
    }
}
    

CLEvent* CLEvent::instantiate(const std::string& tag)
{
    if (tag == std::string("BundleSendRequest"))
        return new CLBundleSendRequest();
    
    else if (tag == std::string("BundleTransmittedEvent"))
        return new CLBundleTransmittedEvent();
    
    else if (tag == std::string("BundleTransmitFailedEvent"))
        return new CLBundleTransmitFailedEvent();
    
    else if (tag == std::string("BundleReceivedEvent"))
        return new CLBundleReceivedEvent();
    
    else if (tag == std::string("BundleHeaderReceivedEvent"))
        return new CLBundleHeaderReceivedEvent();
    
    else if (tag == std::string("BundlePayloadReceivedEvent"))
        return new CLBundlePayloadReceivedEvent();

    else if (tag == std::string("InterfaceCreateRequest"))
        return new CLInterfaceCreateRequest();

    else if (tag == std::string("InterfaceCreatedEvent"))
        return new CLInterfaceCreatedEvent();

    else if (tag == std::string("LinkCreateRequest"))
        return new CLLinkCreateRequest();

    else if (tag == std::string("LinkCreatedEvent"))
        return new CLLinkCreatedEvent();
    
    else if (tag == std::string("LinkStateChangedEvent"))
        return new CLLinkStateChangedEvent();

    else if (tag == std::string("UpEvent"))
        return new CLUpEvent();

    else if (tag == std::string("UpReplyEvent"))
        return new CLUpReplyEvent();

    return NULL;
}


CLEvent::CLEvent(cl_event_type_t type, size_t append_length, u_char* append_data)
{ 
    type_value = type;
    appendedLength = append_length;
    appendedData = append_data;
}


CLEvent::CLEvent(const CLEvent& copy) : SerializableObject()
{
    type_value = copy.type_value;
    appendedLength = copy.appendedLength;
    
    if (appendedLength > 0) {
        appendedData = new u_char[appendedLength];
        memcpy(appendedData, copy.appendedData, appendedLength);
    }
     
    else {
        appendedData = NULL;
    }
}


CLEvent::~CLEvent() 
{
    delete appendedData;
}


void CLEvent::serialize(oasys::SerializeAction* a)
{
    if (a->action_code() == oasys::SerializeAction::UNMARSHAL || appendedLength > 0)
        a->process("appendedLength", &appendedLength);
}


CLUpEvent::CLUpEvent(const std::string& n) : CLEvent(CL_UP)
{
    name = n;
}


void CLUpEvent::serialize(oasys::SerializeAction* a)
{
    a->process("name", &name);
}


CLUpReplyEvent::CLUpReplyEvent(const std::string& eid, const std::string& inDir,
                               const std::string& outDir)
    : CLEvent(CL_UP_REPLY)
{
    localEID = eid;
    incomingBundleDir = inDir;
    outgoingBundleDir = outDir;
}


void CLUpReplyEvent::serialize(oasys::SerializeAction* a)
{
    a->process("localEID", &localEID);
    a->process("incomingBundleDir", &incomingBundleDir);
    a->process("outgoingBundleDir", &outgoingBundleDir);
}


CLInterfaceCreateRequest::CLInterfaceCreateRequest(const std::string& n,
                                                   int argc,
                                                   const char** argv)
    : CLEvent(CL_INTERFACE_CREATE)
{
    name = n;

    for (int arg_i = 0; arg_i < argc; ++arg_i) {
        if (strncmp(argv[arg_i], "protocol=", 9) != 0)
            arguments += std::string(argv[arg_i]) + std::string(" ");
    }
}


void CLInterfaceCreateRequest::serialize(oasys::SerializeAction* a)
{
    a->process("name", &name);
    a->process("arguments", &arguments);
}


CLInterfaceCreatedEvent::CLInterfaceCreatedEvent(const std::string& n)
    : CLEvent(CL_INTERFACE_CREATED)
{
    name = n;
}


void CLInterfaceCreatedEvent::serialize(oasys::SerializeAction* a)
{
    a->process("name", &name);
}


CLInterfaceDestroyRequest::CLInterfaceDestroyRequest(const std::string& n)
    : CLEvent(CL_INTERFACE_DESTROY)
{
    name = n;
}


void CLInterfaceDestroyRequest::serialize(oasys::SerializeAction* a)
{
    a->process("name", &name);
}


CLLinkCreateRequest::CLLinkCreateRequest(Link::link_type_t t,
                                         const std::string& n,
                                         const std::string& eid,
                                         const std::string& nh,
                                         int argc,
                                         const char** argv)
    : CLEvent(CL_LINK_CREATE)
{
    type = t;
    name = n;
    remoteEID = eid;
    nextHop = nh;

    for (int arg_i = 0; arg_i < argc; ++arg_i) {
        if (strncmp(argv[arg_i], "protocol=", 9) != 0)
            arguments += std::string(argv[arg_i]) + std::string(" ");
    }
}


void CLLinkCreateRequest::serialize(oasys::SerializeAction* a)
{
    process_link_type("type", &type, a);
    a->process("name", &name);
    a->process("remoteEID", &remoteEID);
    a->process("nextHop", &nextHop);
    a->process("arguments", &arguments);
}


CLLinkCreatedEvent::CLLinkCreatedEvent(const std::string& n)
    : CLEvent(CL_LINK_CREATED)
{
    name = n;
}


void CLLinkCreatedEvent::serialize(oasys::SerializeAction* a)
{
    a->process("name", &name);
    a->process("nextHop", &nextHop);
    a->process("peerEID", &peerEID);
    a->process("discovered", &discovered);
}


CLLinkOpenRequest::CLLinkOpenRequest(const std::string& n)
    : CLEvent(CL_LINK_OPEN)
{
    name = n;
}


void CLLinkOpenRequest::serialize(oasys::SerializeAction* a)
{
    a->process("name", &name);
}


CLLinkCloseRequest::CLLinkCloseRequest(const std::string& n)
    : CLEvent(CL_LINK_CLOSE)
{
    name = n;
}
 

void CLLinkCloseRequest::serialize(oasys::SerializeAction* a)
{
    a->process("name", &name);
}



CLLinkStateChangedEvent::CLLinkStateChangedEvent(const std::string& n,
                                                 Link::state_t s,
                                                 ContactEvent::reason_t r)
    : CLEvent(CL_LINK_STATE_CHANGED)
{
    name = n;
    newState = s;
    reason = r;
}


void CLLinkStateChangedEvent::serialize(oasys::SerializeAction* a)
{
    std::string temp;
    
    a->process("name", &name);
    
    process_link_state("newState", &newState, a);
    process_link_state_reason("reason", &reason, a);
}


CLBundleSendRequest::CLBundleSendRequest(u_int32_t bid,
                                         const std::string& ln,
                                         const std::string& fn)
    : CLEvent(CL_BUNDLE_SEND)
{
    bundleID = bid;
    linkName = ln;
    filename = fn;
}


void CLBundleSendRequest::serialize(oasys::SerializeAction* a) 
{
    a->process("bundleID", &bundleID);
    a->process("linkName", &linkName);
    a->process("filename", &filename);
}


CLBundleTransmittedEvent::CLBundleTransmittedEvent(u_int32_t bID,
                                                   const std::string& ln,
                                                   size_t bs,
                                                   size_t rs)
    : CLEvent(CL_BUNDLE_TRANSMITTED)
{
    bundleID = bID;
    linkName = ln;
    bytesSent = bs;
    reliablySent = rs;
}


void CLBundleTransmittedEvent::serialize(oasys::SerializeAction* a) 
{
    a->process("bundleID", &bundleID);
    a->process("linkName", &linkName);
    a->process("bytesSent", &bytesSent);
    a->process("reliablySent", &reliablySent);
}


CLBundleTransmitFailedEvent::CLBundleTransmitFailedEvent(u_int32_t bID,
        const std::string& ln) : CLEvent(CL_BUNDLE_TRANSMIT_FAILED)
{
    bundleID = bID;
    linkName = ln;
}


void CLBundleTransmitFailedEvent::serialize(oasys::SerializeAction* a) 
{
    a->process("bundleID", &bundleID);
    a->process("linkName", &linkName);
}


CLBundleReceivedEvent::CLBundleReceivedEvent(const std::string& pf, size_t br)
    : CLEvent(CL_BUNDLE_RECEIVED)
{
    payloadFile = pf;
    bytesReceived = br;
}


void CLBundleReceivedEvent::serialize(oasys::SerializeAction* a) 
{
    a->process("payloadFile", &payloadFile);
    a->process("bytesReceived", &bytesReceived);
    CLEvent::serialize(a);
}


CLBundleHeaderReceivedEvent::CLBundleHeaderReceivedEvent(
        const std::string& link_name)
    : CLEvent(CL_BUNDLE_HEADER_RECEIVED)
{
    linkName = link_name;
}


void CLBundleHeaderReceivedEvent::serialize(oasys::SerializeAction* a) 
{
    a->process("linkName", &linkName);
    CLEvent::serialize(a);
}


CLBundleHeaderReceiveReplyEvent::CLBundleHeaderReceiveReplyEvent(u_int32_t bid,
        const std::string& link_name, const std::string& pf)
    : CLEvent(CL_BUNDLE_HEADER_RECEIVE_REPLY)
{
    bundleID = bid;
    linkName = link_name;
    payloadFileName = pf;
}


void CLBundleHeaderReceiveReplyEvent::serialize(oasys::SerializeAction* a)
{
    a->process("bundleID", &bundleID);
    a->process("linkName", &linkName);
    a->process("payloadFileName", &payloadFileName);
}


CLBundlePayloadReceivedEvent::CLBundlePayloadReceivedEvent(u_int32_t bid,
        const std::string& if_name, size_t br)
    : CLEvent(CL_BUNDLE_PAYLOAD_RECEIVED)
{
    bundleID = bid;
    interfaceName = if_name;
    bytesReceived = br;
}


void CLBundlePayloadReceivedEvent::serialize(oasys::SerializeAction* a)
{
    a->process("bundleID", &bundleID);
    a->process("interfaceName", &interfaceName);
    a->process("bytesReceived", &bytesReceived);
}

} // namespace clevent

} // namespace dtn

#endif // XERCES_C_ENABLED

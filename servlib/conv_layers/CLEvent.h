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

#ifndef _CL_EVENT_H_
#define _CL_EVENT_H_

#include <config.h>
#ifdef XERCES_C_ENABLED

#include <string>

#include <oasys/serialize/Serialize.h>

#include <contacts/Link.h>
#include <bundling/BundleEvent.h>

namespace dtn {

namespace clevent {

typedef enum {
    CL_BUNDLE_RECEIVED = 0x1,  ///< New bundle arrival
    CL_BUNDLE_HEADER_RECEIVED,
    CL_BUNDLE_HEADER_RECEIVE_REPLY,
    CL_BUNDLE_PAYLOAD_RECEIVED,
    CL_BUNDLE_SEND,
    CL_BUNDLE_TRANSMITTED,     ///< Bundle or fragment successfully sent
    CL_BUNDLE_TRANSMIT_FAILED, ///< Bundle or fragment successfully sent

    CL_LINK_CREATED,       ///< Link is created into the system
    CL_LINK_DELETED,       ///< Link is deleted from the system
    CL_LINK_STATE_CHANGED,
    CL_LINK_OPEN,
    CL_LINK_CLOSE,
    CL_LINK_AVAILABLE,     ///< Link is available
    CL_LINK_UNAVAILABLE,       ///< Link is unavailable
    CL_LINK_CREATE,                ///< Create and open a new link

    CL_INTERFACE_CREATE,
    CL_INTERFACE_CREATED,
    CL_INTERFACE_DESTROY,
    CL_INTERFACE_DELETED,

    CL_UP,
    CL_UP_REPLY,
    CL_DOWN,

} cl_event_type_t;

class CLEvent : public oasys::SerializableObject {
public:
    CLEvent(const CLEvent& copy);
    virtual ~CLEvent();

    cl_event_type_t type() const { return type_value; }
    
    virtual CLEvent* clone() const = 0;
    virtual void serialize(oasys::SerializeAction* a);
    virtual std::string event_name() const { return std::string("clEvent"); }
    std::string serialize_name() const { return event_name(); }

    static CLEvent* instantiate(const std::string& tag);
    
    size_t appendedLength;
    u_char* appendedData;

protected:
    CLEvent(cl_event_type_t type, size_t append_length = 0,
            u_char* append_data = NULL);

private:
    cl_event_type_t type_value;
};


/** Announce that a new external convergence layer has started.
 * 
 * This should be the first message sent by the CLA, and the CLA should expect
 * to see CLUpReplyEvent shortly thereafter.
 * 
 * @param name - The name of the CLA, usually the protocol that it supports.
 */
class CLUpEvent : public CLEvent {
public:
    CLUpEvent() : CLEvent(CL_UP) { }
    CLUpEvent(const std::string& n);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLUpEvent(*this); }
    std::string event_name() const { return std::string("UpEvent"); }

    std::string name;
};


/** Reply from the BPA to a CLUpEvent.
 * 
 * This is the first message sent by the BPA after receiving a CLUpEvent. The
 * CLA should not send any messages until it receives this.
 * 
 * @param localEID - The EID of the node on which the BPA and CLA are running.
 * @param payloadDir - The directory (absolute path) where bundles are located.
 */
class CLUpReplyEvent : public CLEvent {
public:
    CLUpReplyEvent() : CLEvent(CL_UP_REPLY) { }
    CLUpReplyEvent(const std::string& eid, const std::string& inDir,
                   const std::string& outDir);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLUpReplyEvent(*this); }
    std::string event_name() const { return std::string("UpReplyEvent"); }

    std::string localEID;
    std::string incomingBundleDir;
    std::string outgoingBundleDir;
};


/** Request from the BPA for the CLA to create an interface.
 * 
 * When the interface is created, the CLA should reply with a
 * CLInterfaceCreatedEvent.
 * 
 * @todo  There should be a CLInterfaceCreationFailedEvent.
 * 
 * @param name  The name of the interface.
 * @param arguments  The CL-specific arguments for this interface.
 */
class CLInterfaceCreateRequest : public CLEvent {
public:
    CLInterfaceCreateRequest() : CLEvent(CL_INTERFACE_CREATE) { }
    CLInterfaceCreateRequest(const std::string& n, int argc, const char** argv);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLInterfaceCreateRequest(*this); }
    std::string event_name() const { return std::string("InterfaceCreateRequest"); }

    std::string name;
    std::string arguments;
};


/** Response from the CLA when an interface has been created.
 * 
 * This should be sent in reply to CLInterfaceCreateRequest when the interface
 * has been successfully created.
 * 
 * @param name  The name of the interface, as given in the corresponding
 *              CLInterfaceCreateRequest.
 */
class CLInterfaceCreatedEvent : public CLEvent {
public:
    CLInterfaceCreatedEvent() : CLEvent(CL_INTERFACE_CREATED) { }
    CLInterfaceCreatedEvent(const std::string& n);

    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLInterfaceCreatedEvent(*this); }
    std::string event_name() const { return std::string("InterfaceCreatedEvent"); }

    std::string name;
};


/** Request from the BPA for the CLA to close down and destroy an interface.
 * 
 * @param name  The name of the interface to close.
 */
class CLInterfaceDestroyRequest : public CLEvent {
public:
    CLInterfaceDestroyRequest() : CLEvent(CL_INTERFACE_DESTROY) { }
    CLInterfaceDestroyRequest(const std::string& n);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLInterfaceDestroyRequest(*this); }
    std::string event_name() const { return std::string("InterfaceDestroyRequest"); }

    std::string name;
};


/** Request from the BPA for the CLA to create a link.
 * 
 * The CLA should create the link and reply with CLLinkCreatedEvent.
 * 
 * @todo  There should be a CLLinkCreationFailedEvent.
 * 
 * @param type  The type of link to create.
 * @param name  The name of the link.
 * @param nextHop  The CL-specific string for the next hop in the link.
 * @param arguments  The CL-specific arguments for this link.
 */
class CLLinkCreateRequest : public CLEvent {
public:
    CLLinkCreateRequest() : CLEvent(CL_LINK_CREATE) { }
    CLLinkCreateRequest(Link::link_type_t t,
                        const std::string& n,
                        const std::string& eid,
                        const std::string& nh,
                        int argc, const char** argv);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLLinkCreateRequest(*this); }
    std::string event_name() const { return std::string("LinkCreateRequest"); }

    Link::link_type_t type;
    std::string name;
    std::string remoteEID;
    std::string nextHop;
    std::string arguments;
};


/** Response from the CLA when a link has been created.
 * 
 * This should be sent either in reply to CLLinkCreateRequest when the link
 * has been successfully created, or when the CLA has discovered a new link. If
 * this link was discovered by the CLA, the 'discovered' attribute should be
 * 'true'.
 * 
 * @param name  The name of the link.
 * @param nextHop  If the link is discovered, this is the next hop for the link.
 *                 Otherwise, this attribute is unused.
 * @param peerEID  If the link is discovered, this is the EID of the other node
 *                 on the link. Otherwise, this attribute is unused.
 * @param discovered  If 'true', this link was discovered by the CLA; if
 *                    'false', this message is in response to a
 *                    CLLinkCreateRequest.
 */
class CLLinkCreatedEvent : public CLEvent {
public:
    CLLinkCreatedEvent() : CLEvent(CL_LINK_CREATED) { }
    CLLinkCreatedEvent(const std::string& n);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLLinkCreatedEvent(*this); }
    std::string event_name() const { return std::string("LinkCreatedEvent"); }

    std::string name;
    std::string nextHop;
    std::string peerEID;
    bool discovered;
};


/** Request from the BPA for the CLA to attempt to open a link.
 * 
 * This message should only be sent for a given link after the
 * CLLinkCreatedEvent has been received for that link. If the CLA is successful
 * in opening the link, a CLLinkStateChangedEvent with a 'newState' of 'open'
 * will be sent back.
 * 
 * @param name  The name of the link to open.
 */
class CLLinkOpenRequest : public CLEvent {
public:
    CLLinkOpenRequest() : CLEvent(CL_LINK_OPEN) { }
    CLLinkOpenRequest(const std::string& n);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLLinkOpenRequest(*this); }
    std::string event_name() const { return std::string("LinkOpenRequest"); }

    std::string name;
};


/** Request from the BPA for the CLA to close a link.
 * 
 * The CLA will close the link and send a CLLinkStateChangedEvent with a 
 * 'newState' of 'closed'.
 * 
 * @param name  The name of the link to close.
 */
class CLLinkCloseRequest : public CLEvent {
public:
    CLLinkCloseRequest() : CLEvent(CL_LINK_CLOSE) { }
    CLLinkCloseRequest(const std::string& n);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLLinkCloseRequest(*this); }
    std::string event_name() const { return std::string("LinkCloseRequest"); }

    std::string name;
};


/** Notice from the CLA to the BPA that the state of a link has changed.
 * 
 * The change may be the result of current network conditions, a closure
 * after an idle period, or a CLLinkOpenRequest or CLLinkCloseRequest from the
 * BPA.
 * 
 * @param name  The name of the link whose state has changed.
 * @param newState  The new state of the link.
 * @param reason  The reason for the change in state.
 */
class CLLinkStateChangedEvent : public CLEvent {
public:
    CLLinkStateChangedEvent() : CLEvent(CL_LINK_STATE_CHANGED) { }
    CLLinkStateChangedEvent(const std::string& n, Link::state_t s,
                            ContactEvent::reason_t r);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLLinkStateChangedEvent(*this); }
    std::string event_name() const { return std::string("LinkStateChangedEvent"); }

    std::string name;
    Link::state_t newState;
    ContactEvent::reason_t reason;
};


/** Request from the BPA for the CLA to send a bundle.
 * 
 * @todo  Decide how bundle and headers are moving.
 * 
 * @param bundleID  The ID given to the bundle by the BPA.
 * @param linkName  The name of the link on which to send the bundle.
 * @param payloadFile
 */
class CLBundleSendRequest : public CLEvent {
public:
    CLBundleSendRequest() : CLEvent(CL_BUNDLE_SEND) { }
    CLBundleSendRequest(u_int32_t bid, const std::string& ln,
                        const std::string& fn);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLBundleSendRequest(*this); }
    std::string event_name() const { return std::string("BundleSendRequest"); }

    u_int32_t bundleID;
    std::string linkName;
    std::string filename;
};


/**
 */
class CLBundleTransmittedEvent : public CLEvent {
public:
    CLBundleTransmittedEvent() : CLEvent(CL_BUNDLE_TRANSMITTED) { }
    CLBundleTransmittedEvent(u_int32_t bID, const std::string& ln, size_t bs,
                             size_t rs);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLBundleTransmittedEvent(*this); }
    std::string event_name() const { return std::string("BundleTransmittedEvent"); }

    u_int32_t bundleID;
    std::string linkName;
    size_t bytesSent;
    size_t reliablySent;
};


class CLBundleTransmitFailedEvent : public CLEvent {
public:
    CLBundleTransmitFailedEvent() : CLEvent(CL_BUNDLE_TRANSMIT_FAILED) { }
    CLBundleTransmitFailedEvent(u_int32_t bID, const std::string& ln);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLBundleTransmitFailedEvent(*this); }
    std::string event_name() const { return std::string("BundleTransmitFailedEvent"); }

    u_int32_t bundleID;
    std::string linkName;
};


class CLBundleReceivedEvent : public CLEvent {
public:
    CLBundleReceivedEvent() : CLEvent(CL_BUNDLE_RECEIVED) { }
    CLBundleReceivedEvent(const std::string& pf, size_t br);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLBundleReceivedEvent(*this); }
    std::string event_name() const { return std::string("BundleReceivedEvent"); }

    std::string payloadFile;
    size_t bytesReceived;
};


class CLBundleHeaderReceivedEvent : public CLEvent {
public:
    CLBundleHeaderReceivedEvent() : CLEvent(CL_BUNDLE_HEADER_RECEIVED) { }
    CLBundleHeaderReceivedEvent(const std::string& link_name);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLBundleHeaderReceivedEvent(*this); }
    std::string event_name() const { return std::string("BundleHeaderReceivedEvent"); }

    std::string linkName;
};


class CLBundleHeaderReceiveReplyEvent : public CLEvent {
public:
    CLBundleHeaderReceiveReplyEvent() : CLEvent(CL_BUNDLE_HEADER_RECEIVE_REPLY) { }
    CLBundleHeaderReceiveReplyEvent(u_int32_t bid, const std::string& link_name,
                                    const std::string& pf);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLBundleHeaderReceiveReplyEvent(*this); }
    std::string event_name() const { return std::string("BundleHeaderReceiveReplyEvent"); }

    u_int32_t bundleID;
    std::string linkName;
    std::string payloadFileName;
};


class CLBundlePayloadReceivedEvent : public CLEvent {
public:
    CLBundlePayloadReceivedEvent() : CLEvent(CL_BUNDLE_PAYLOAD_RECEIVED) { }
    CLBundlePayloadReceivedEvent(u_int32_t bid, const std::string& if_name,
                                 size_t br);
    
    void serialize(oasys::SerializeAction* a);
    CLEvent* clone() const { return new CLBundlePayloadReceivedEvent(*this); }
    std::string event_name() const { return std::string("BundlePayloadReceivedEvent"); }

    u_int32_t bundleID;
    std::string interfaceName;
    size_t bytesReceived;
};

} // namespace clevent

} // namespace dtn

#endif // XERCES_C_ENABLED
#endif

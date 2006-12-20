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

#ifndef _ECLMODULE_H_
#define _ECLMODULE_H_

#include <config.h>
#ifdef XERCES_C_ENABLED

#include <semaphore.h>
#include <string>
#include <list>
#include <ext/hash_map>

#include <oasys/thread/Thread.h>
#include <oasys/thread/Mutex.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/io/TCPClient.h>
#include <oasys/serialize/XercesXMLSerialize.h>

#include "ExternalConvergenceLayer.h"
#include "CLEventHandler.h"

namespace dtn {

using __gnu_cxx::hash_multimap;
using __gnu_cxx::hash;


/** Handles communication to and from an external module.
 *
 * There is one ECLModule thread for each external module that connects to DTN2.
 * Messages can be sent to the module with post_event(CLEvent*) method.
 */
class ECLModule : public CLInfo,
                  public CLEventHandler,
                  public oasys::Thread {

//ExternalConvergenceLayer must be able to check for a link
friend class ExternalConvergenceLayer;

public:
    /** Construct a new ECLModule after the module initiates contact.
     *
     * @param fd - The socket descriptor that the module connected on.
     * @param remote_addr - The address of the connecting module (probably
     *      localhost).
     * @param remote_port - The port number of the connecting module.
     * @param cl - A reference back to the convergence layer.
     */
    ECLModule(int fd, in_addr_t remote_addr, u_int16_t remote_port,
              ExternalConvergenceLayer& cl);
    virtual ~ECLModule();

    virtual void run();


    /** Post an event for transmission to the external module.
     *
     * @param event - The message to be transmitted.  This class assumes
     *      responsibility for deleting the event when it is no longer needed.
     */
    void post_event(CLEvent* event);
    
    
    void take_resource(ECLResource* resource);


    /** Remove an interface from the list of active interfaces.
     *
     * This should be called after sending a CLInterfaceDestroyRequest to the
     * module in order to clean up any resources used locally for the interface.
     * The interface will not be given back to the unclaimed resource list, so
     * it will not reappear if the module restarts unless it is explicitly
     * created again.
     *
     * @param name - The name of the interface to remove.
     *
     * @return A pointer to the ECLInterfaceResource that was just removed, or
     *      NULL if the specified interface does not exist.  This pointer should
     *      be deleted by the caller.
     */
    ECLInterfaceResource* remove_interface(const std::string& name);

    
    /** Retrieve the module's name, which should be the name of the protocol
     * that it supports.
     */
    const std::string& name() const { return name_; }

protected:
    void handle_cl_up(CLUpEvent* event);
    void handle_cl_interface_created(CLInterfaceCreatedEvent* event);
    void handle_cl_link_created(CLLinkCreatedEvent* event);
    void handle_cl_link_state_changed(CLLinkStateChangedEvent* event);
    void handle_cl_bundle_transmitted(CLBundleTransmittedEvent* event);
    void handle_cl_bundle_transmit_failed(CLBundleTransmitFailedEvent* event);
    void handle_cl_bundle_received(CLBundleReceivedEvent* event);

    /** Take resources from the resource list that belong to this module.
     */
    void take_resources();

    /** Check if a link exists on either link list.
     */
    bool link_exists(const std::string& name) const;

private:
    /// The size of the buffer used by read_cycle().
    static const size_t READ_BUFFER_SIZE = 256;
    
    /// The maximum amount of the bundle that we will map when sending to and
    /// receiving from the module.
    static const size_t MAX_BUNDLE_IN_MEMORY = (256 * 1024);


    /** Make the next pass when there is input on the socket.
     *
     * As input comes in on the socket, this will make one pass of at most
     * READ_BUFFER_SIZE bytes.  Based on the current state of things, this could
     * read the XML document, parse the document into a CLEvent, read any
     * appended data, and dispatch the completed CLEvent.  The general idea is
     * that this function should just keep getting called from the main loop
     * whenever input is available and it will do the above actions as it needs
     * to and is able to.
     * 
     * @todo Since we are not passing the bundle headers over the socket, this
     *       may be unnecessarily complex.
     */
    void read_cycle();


    /** Send a message to the external module.
     *
     * This will build the XML document from the CLEvent and send it and any
     * appended data out on the socket.
     *
     * @param event - The event to send.  Note that this pointer is not deleted
     *      by this method.
     *
     * @return 0 if the message was successfully sent, -1 if it was not.
     */
    int send_event(CLEvent* event);
    
    /** Prepare a bundle to be sent by the CLA.
     * 
     * This must be called before a CLBundleSendRequest can be sent by
     * send_event(). The entire bundle (all headers and payload) will be
     * written to a file in bundle_out_path_ exactly as it should be sent by
     * the CLA. The payloadFile attribute of 'event' will be filled with the
     * name of this file (relative to bundle_out_path_).
     * 
     */
    int prepare_bundle_to_send(CLBundleSendRequest* send_request);
    
    
    /** Clean up after a bundle when it cannot be sent.
     * 
     * This is called by prepare_bundle_to_send() when something goes wrong
     * and by cleanup() when a module dies while it still has bundles waiting
     * to be sent. It will remove the bundle from the link's outgoing list (if
     * erase_from_list is true), erase the bundle file from storage, and post
     * a BundleTransmitFailedEvent to the BundleDaemon.
     * 
     * @note The 'path' field of outgoing_bundle must be set before calling
     *       this method.
     * 
     * @param link_resource  The holder for the link on which this bundle was
     *                       to be sent.
     * @param outgoing_bundle  The holder for the bundle that was not sent.
     * @param erase_from_list  If true, erase this bundle from link_resource's
     *                         outgoing bundle list.
     * 
     */
    void bundle_send_failed(ECLLinkResource* link_resource,
                            OutgoingBundle* outgoing_bundle,
                            bool erase_from_list); 

    /** Retrieve an interface from the interface list.
     *
     * @param name - The name of the interface to find.
     * @return The ECLInterfaceResource for the requested interface, or NULL
     *         if it does not exist.
     */
    ECLInterfaceResource* get_interface(const std::string& name) const;


    /** Retrieve a link from the link lists.
     *
     * @param name - The name of the link to find.
     * @return The ECLLinkResource for the requested link, or NULL
     *         if it does not exist on either normal_links_ or temp_links_.
     */
    ECLLinkResource* get_link(const std::string& name) const;


    /** Create a link in response to another node's link creation to us.
     *
     * This is called when a CLLinkCreatedEvent has the 'discovered' field
     * set true, which means that another node has opened a link to us
     * and we need one to go back the other way.
     *
     * The created ECLLinkResource is placed on temp_link_list_, rather
     * than link_list_, so that we do not try to give it back to the CL
     * when the external module closes.
     *
     * @param peer_eid - The EID of the node that opened the connection to us.
     * @param nexthop - The next hop to the other node.
     * @param link_name - The name of the link, specified by the external
     *                    module.
     * @return The ECLLinkResource created for this link. This resource
     *         has been placed in temp_link_list_.
     */
    ECLLinkResource* create_discovered_link(const std::string& peer_eid,
                                            const std::string& nexthop,
                                            const std::string& link_name);


    /** Clean up state for this module after loosing contact with it.
     *
     * This will do the work of closing down links, returning resources, and
     * canceling bundles.  All links will be made CLOSED with a
     * LinkStateChangeRequest. Any ECLResources that we own (in iface_list_ and
     * normal_links_) will be returned to ExternalConvergenceLayer's unclaimed
     * resource list. Links in temp_links_ will be destroyed. For any bundle
     * sitting in the outgoing list of a link, a BundleTransmitFailedEvent will
     * be sent to the BundleDaemon. Finally, the incoming and outgoing bundle
     * directories will be removed.
     */
    void cleanup();
    
    unsigned next_bundle_id_;
    
    /// This module's protocol name.
    std::string name_;

    /// The stuff that we put at the begining of XML documents with schema
    /// name, XML version, etc.
    std::string xml_header_;
    
    /// The path where bundles going out to the CLA are stored.
    std::string bundle_in_path_;
    
    /// The path where bundles comming in from the CLA are stored.
    std::string bundle_out_path_;

    /// The end-of-data index into the appendedData buffer of event_.
    size_t append_i_;

    /// The event currently being processed by read_cycle().
    CLEvent* event_;

    /// Raw data buffer for read_cycle().
    char read_buffer_[READ_BUFFER_SIZE];

    /// String buffer for read_cycle().
    std::vector<char> msg_buffer_;

    /// Reference back to the convergence layer.
    ExternalConvergenceLayer& cl_;

    /// The interface list.
    std::list<ECLInterfaceResource*> iface_list_;
    
    /// Mutex protecting iface_list_.
    oasys::Mutex iface_list_lock_;

    /// The list of normal (non-temporary) links.
    LinkHashMap normal_links_;

    /// The list of temporary/discovered links
    LinkHashMap temp_links_;
    
    /// Semaphore for normal_links_ and temp_links_. It is OK for two threads
    /// to read either of these at the same time, but they must be locked
    /// when makeing changes to them.
    mutable sem_t link_list_sem_;

    /// The socket to the external module.
    oasys::TCPClient socket_;

    /// The queue for messages to be sent to the external module.
    oasys::MsgQueue<CLEvent*> event_queue_;

    /// The parser used to parse XML messages from the external module.
    oasys::XercesXMLUnmarshal parser_;
};

} // namespace dtn

#endif // XERCES_C_ENABLED
#endif // _ECLMODULE_H_

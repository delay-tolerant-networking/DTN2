/* Copyright 2004-2006 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * $Id$
 */

#include <config.h>
#ifdef XERCES_C_ENABLED

#include <oasys/io/NetUtils.h>
#include <oasys/io/FileUtils.h>
#include <oasys/io/TCPClient.h>
#include <oasys/io/IO.h>
#include <oasys/thread/Lock.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/serialize/XMLSerialize.h>
#include <oasys/thread/SpinLock.h>

#include <xercesc/framework/MemBufFormatTarget.hpp>

#include "ECLModule.h"
#include "bundling/BundleDaemon.h"
#include "storage/BundleStore.h"
#include "storage/GlobalStore.h"
#include "contacts/ContactManager.h"

 
namespace dtn {

const size_t ECLModule::READ_BUFFER_SIZE;
const size_t ECLModule::MAX_BUNDLE_IN_MEMORY;

ECLModule::ECLModule(int fd,
                     in_addr_t remote_addr,
                     u_int16_t remote_port,
                     ExternalConvergenceLayer& cl) :
    CLEventHandler("ECLModule", "/dtn/cl/module"),
    Thread("/dtn/cl/module", Thread::DELETE_ON_EXIT),
    cl_(cl),
    iface_list_lock_("/dtn/cl/parts/iface_list_lock"),
    socket_(fd, remote_addr, remote_port, logpath_),
    message_queue_("/dtn/cl/parts/module"),
    parser_( true, cl.schema_.c_str() )
{
    name_ = "(unknown)";
    sem_init(&link_list_sem_, 0, 2);
}

ECLModule::~ECLModule()
{
    while (message_queue_.size() > 0)
        delete message_queue_.pop_blocking();
}

void
ECLModule::run()
{
    struct pollfd pollfds[2];

    struct pollfd* message_poll = &pollfds[0];
    message_poll->fd = message_queue_.read_fd();
    message_poll->events = POLLIN;

    struct pollfd* sock_poll = &pollfds[1];
    sock_poll->fd = socket_.fd();
    sock_poll->events = POLLIN;

    while ( !should_stop() ) {
        // Poll for activity on either the event queue or the socket.
        int ret = oasys::IO::poll_multiple(pollfds, 2, -1);

        if (ret == oasys::IOINTR) {
            log_debug("Module server interrupted");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_debug("Module server error");
            set_should_stop();
            continue;
        }

        if (message_poll->revents & POLLIN) {
            cl_message* message;
            if ( message_queue_.try_pop(&message) ) {
                ASSERT(message != NULL);
                int result;
                
                // We need to handle bundle-send messages as a special case,
                // in order to get the bundle written to disk first.
                if ( message->bundle_send_request().present() )
                    result = prepare_bundle_to_send(message);
                
                else
                    result = send_message(message);
                
                delete message;

                if (result < 0) {
                    set_should_stop();
                    continue;
                } // if
            } // if
        } // if

        // Check for input on the socket and read whatever is available.
        if (sock_poll->revents & POLLIN)
            read_cycle();
    } // while

    log_info( "CL %s is shutting down", name_.c_str() );
    
    oasys::ScopeLock lock(&cl_.global_resource_lock_, "ECLModule::run");
    cl_.remove_module(this);
    socket_.close();
    cleanup();
}

void
ECLModule::post_message(cl_message* message)
{
    message_queue_.push_back(message);    
}

ECLInterfaceResource*
ECLModule::remove_interface(const std::string& name)
{
    oasys::ScopeLock lock(&iface_list_lock_, "remove_interface");
    std::list<ECLInterfaceResource*>::iterator iface_i;

    for (iface_i = iface_list_.begin(); iface_i != iface_list_.end(); ++iface_i) {
        if ( (*iface_i)->interface->name() == name) {
            iface_list_.erase(iface_i);
            return *iface_i;
        }
    }

    return NULL;
}

void
ECLModule::handle(const cla_add_request& message)
{
    if (cl_.get_module( message.name() ) != NULL) {
        log_err("A CLA with name '%s' already exists", message.name().c_str());
        set_should_stop();
        return;
    }
    
    name_ = message.name();
    logpathf( "/dtn/cl/%s", name_.c_str() );
    message_queue_.logpathf( "/dtn/cl/parts/%s/message_queue", name_.c_str() );
    socket_.logpathf( "/dtn/cl/parts/%s/socket", name_.c_str() );
    
    log_info( "New external CL: %s", name_.c_str() );

    // Figure out the bundle directory paths.
    BundleStore* bs = BundleStore::instance();
    std::string payload_dir = bs->payload_dir();
    oasys::FileUtils::abspath(&payload_dir);
    
    oasys::StringBuffer in_dir( "%s/%s-in", payload_dir.c_str(),
                                name_.c_str() );
    oasys::StringBuffer out_dir( "%s/%s-out", payload_dir.c_str(),
                                 name_.c_str() );
    
    // Save the bundle directory paths.
    bundle_in_path_ = std::string( in_dir.c_str() );
    bundle_out_path_ = std::string( out_dir.c_str() );
    
    // Delete the module's incoming and outgoing bundle directories just in
    // case the already exist and contain stale bundle files.
    ::remove( bundle_in_path_.c_str() );
    ::remove( bundle_out_path_.c_str() );
    
    // Create the incoming bundle directory.
    if (oasys::IO::mkdir(in_dir.c_str(), 0777) < 0) {
        log_err( "Unable to create incoming bundle directory %s: %s",
                 in_dir.c_str(), strerror(errno) );
        
        set_should_stop();
        return;
    }
    
    // Create the outgoing bundle directory.
    if (oasys::IO::mkdir(out_dir.c_str(), 0777) < 0) {
        log_err( "Unable to create outgoing bundle directory %s: %s",
                 out_dir.c_str(), strerror(errno) );
        
        set_should_stop();
        return;
    }
    
    cla_set_params_request request;
    request.local_eid( BundleDaemon::instance()->local_eid().str() );
    request.bundle_pass_method(bundlePassMethodType::filesystem);
    
    ParamSequence params;
    params.push_back( Parameter("incoming_bundle_dir", in_dir.c_str() ) );
    params.push_back( Parameter("outgoing_bundle_dir", out_dir.c_str() ) );
    request.Parameter(params);
    
    POST_MESSAGE(this, cla_set_params_request, request);
    
    // take appropriate resources for this CLA module
    take_resources();
}

void 
ECLModule::take_resource(ECLResource* resource) 
{
    // Handle an Interface.
    if ( typeid(*resource) == typeid(ECLInterfaceResource) ) {
        ECLInterfaceResource* iface = (ECLInterfaceResource*)resource;
        
        iface_list_lock_.lock("take_resource");
        iface_list_.push_back(iface);
        iface_list_lock_.unlock();

        log_info( "Module %s acquiring interface %s", name_.c_str(),
                  iface->interface->name().c_str() );
        
        cl_message* message = new cl_message(*iface->create_message);
        post_message(message);
    }

    // Handle a Link.
    else if ( typeid(*resource) == typeid(ECLLinkResource) ) {
        ECLLinkResource* link = (ECLLinkResource*)resource;
            
        sem_wait(&link_list_sem_);
        sem_wait(&link_list_sem_);
        
        link_list_.insert( LinkHashMap::value_type(link->link->name_str(),
                              link) );
        
        sem_post(&link_list_sem_);
        sem_post(&link_list_sem_);

        log_info( "Module %s acquiring link %s", name_.c_str(),
                  link->link->name() );
        cl_message* message = new cl_message(*link->create_message);
        post_message(message);
    }
    
    else {
        log_err( "Cannot take unknown resource type %s",
                 typeid(*resource).name() );
    } 
}

void
ECLModule::take_resources()
{
	log_info("Module %s is acquiring appropriate CL resources", name_.c_str());
	// Find all existing resources that belong to this CLA.
    std::list<ECLResource*> resource_list_ = cl_.take_resources(name_, this);
    std::list<ECLResource*>::iterator resource_i;

    for (resource_i = resource_list_.begin();
    resource_i != resource_list_.end(); ++resource_i) {
        ECLResource* resource = (*resource_i);
        take_resource(resource);
    }
}

void
ECLModule::handle(const interface_created_event& message)
{
    ECLInterfaceResource* resource = get_interface( message.interface_name() );

    if (!resource) {
        log_warn( "Got interface_created_event for unknown interface %s",
                  message.interface_name().c_str() );
        return;
    }
}

void
ECLModule::handle(const link_created_event& message)
{
    ECLLinkResource* resource;
    LinkRef link;
    const clmessage::linkAttributes& link_attribs = message.linkAttributes();
    
    resource = get_link( message.link_name() );

    // If we get an opportunistic link that is not in our list yet, it was
    // discovered by the CLA, and we should create the link for it.
    if (!resource && link_attribs.type() == linkTypeType::opportunistic) {
        resource = create_discovered_link( link_attribs.peer_eid(),
                                           link_attribs.underlay_address(),
                                           message.link_name() );
        return;
    }
    
    if (!resource) {
        log_err( "Got link_created_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }
    
    link = BundleDaemon::instance()->contactmgr()->
            find_link( message.link_name().c_str() );

    if (link == NULL) {
        // The link is not in the ContactManager. This should mean it has
        // been deleted.

        // XXX there may be some steps to take to handle the link having been
        // deleted, but probably they have already been done at deletion time
    }
    else {
        // This means that the link was successfully added!
        // Also, the only code that checks the CREATE_PENDING flag has
        // already completed (add_new_link must complete before has_link()
        // can return) so resetting it here is safe.
        //link->clear_flag(Link::CREATE_PENDING);
        
        if (link->state() == Link::UNAVAILABLE)
            link->set_state(Link::AVAILABLE);
        
        BundleDaemon::post(new LinkCreatedEvent(link));
    }
}

void
ECLModule::handle(const link_opened_event& message)
{
    ECLLinkResource* resource = get_link( message.link_name() );
    
    if (!resource) {
        log_err( "Got link_opened_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }
    
    ContactRef contact = resource->link->contact();
    if (contact == NULL) {
        contact = new Contact(resource->link);
        resource->link->set_contact( contact.object() );
    }
    
    BundleDaemon::post( new ContactUpEvent(contact) );
}

void
ECLModule::handle(const link_closed_event& message)
{
    ECLLinkResource* resource = get_link( message.link_name() );
    
    if (!resource) {
        log_err( "Got link_closed_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }
    
    resource->known_state = Link::CLOSED;
    resource->link->set_state(Link::UNAVAILABLE);
    
    ContactRef contact = resource->link->contact();
    if (contact != NULL) {
        BundleDaemon::post( 
                new ContactDownEvent(contact, ContactEvent::NO_INFO) );
    }
}

void
ECLModule::handle(const link_state_changed_event& message)
{
    Link::state_t new_state;
    ECLLinkResource* resource = get_link( message.link_name() );

    if (!resource) {
        log_err( "Got link_state_changed_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }
    
    new_state = XMLConvert::convert_link_state( message.new_state() );

    resource->known_state = new_state;
    BundleDaemon::post( new LinkStateChangeRequest( resource->link,
        new_state, XMLConvert::convert_link_reason( message.reason() ) ) );
}

void
ECLModule::handle(const link_deleted_event& message)
{
    ECLLinkResource* resource;

    resource = get_link( message.link_name() );
    
    // If we didn't find this link, check with ExternalConvergenceLayer
    // for anything recently added.
    if (!resource) {
        log_err( "Got link_created_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }
    
    // We need to actually lock the list to erase an element (normally, neither
    // thread will lock on this just to read it).
    sem_wait(&link_list_sem_);
    sem_wait(&link_list_sem_);
    
    link_list_.erase( message.link_name() );
        
    // Unlock the lists.
    sem_post(&link_list_sem_);
    sem_post(&link_list_sem_);
    
    // If the link's cl_info is still set, then the deletion originated at the
    // CLA, not the BPA, and we need to send up a LinkDeleteRequest
    // to get the link removed. Setting the module field NULL will cause
    // delete_link to just delete the resource without sending a request back
    // down here.
    if (resource->link->cl_info() != NULL) {
        resource->module = NULL;
        /*ContactEvent::reason_t reason = XMLConvert::convert_link_reason(
        	message.reason() );
        BundleDaemon::instance()->contactmgr()->del_link(resource->link,
            reason);*/
    }
    
    // Otherwise, we are done with this resource now.
    // NOTE: The ContactManager posts a LinkDeletedEvent, so we do not need
    // another one.
    else {
        delete resource;
    }
}

void
ECLModule::handle(const bundle_transmitted_event& message)
{
    // Find the link that this bundle was going to.
    ECLLinkResource* resource = get_link( message.link_name() );
    if (!resource) {
        log_err( "Got bundle_transmitted_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }

    BundleRef bundle =
            resource->get_outgoing_bundle( message.bundleAttributes() );
    if ( !bundle.object() ) {
        log_err("Got bundle_transmitted_event for unknown bundle");
        return;
    }
    
    // Take this off the outgoing bundle list for this link.
    resource->erase_outgoing_bundle( bundle.object() );
    
    // Figure out the absolute path to the file.
    oasys::StringBuffer filename("bundle%d", bundle->bundleid_);
    std::string abs_path = bundle_out_path_ + "/" + filename.c_str();
    
    // Delete the bundle file.
    ::remove( abs_path.c_str() );

    // Tell the BundleDaemon about this.
    BundleTransmittedEvent* b_event =
            new BundleTransmittedEvent(bundle.object(),
                                       resource->link->contact(),
                                       resource->link,
                                       message.bytes_sent(),
                                       message.reliably_sent());
    BundleDaemon::post(b_event);    
}

void
ECLModule::handle(const bundle_cancelled_event& message)
{
    // Find the link that this bundle was going to.
    ECLLinkResource* resource = get_link( message.link_name() );
    if (!resource) {
        log_err( "Got bundle_cancelled_event for unknown link %s",
                 message.link_name().c_str() );
        return;
    }

    // Find this bundle on the link's outgoing bundle list.
    BundleRef bundle =
            resource->get_outgoing_bundle( message.bundleAttributes() );
    if ( !bundle.object() ) {
        log_err("Got bundle_cancelled_event for unknown bundle");
        return;
    }
    
    // Clean up after the bundle and tell the BPA about it.
    bundle_send_failed(resource, bundle.object(), true);
    /*BundleDaemon::post( new BundleSendCancelledEvent(bundle.object(),
                resource->link) );*/
}

void
ECLModule::handle(const bundle_received_event& message)
{
    int bundle_fd;
    bool finished = false;
    size_t file_offset = 0;
    struct stat file_stat;
    
    std::string file_path = bundle_in_path_ + "/" + message.location();
    
    // Open up the file.
    bundle_fd = oasys::IO::open(file_path.c_str(), O_RDONLY);
    if (bundle_fd < 0) {
        log_err( "Unable to read bundle file %s: %s", file_path.c_str(),
                 strerror(errno) );
        return;
    }
    
    // Stat the file so we know how big it is.
    if (oasys::IO::stat(file_path.c_str(), &file_stat) < 0) {
        log_err( "Unable to stat bundle file %s: %s", file_path.c_str(),
                 strerror(errno) );
        oasys::IO::close(bundle_fd);
        return;
    }
    
    Bundle* bundle = new Bundle();
    
    // Keep feeding BundleProtocol::consume() chunks until either it indicates
    // that the bundle is finished or we run out of bytes in the file (these
    // two SHOULD happen at the same time). This loop is to ensure that only
    // MAX_BUNDLE_IN_MEMORY bytes are actually mapped in memory at a time.
    while (!finished && file_offset < (size_t)file_stat.st_size) {
        size_t map_size = std::min( (size_t)file_stat.st_size - file_offset,
                                    MAX_BUNDLE_IN_MEMORY );
        
        // Map the next chunk of file.
        void* bundle_ptr = oasys::IO::mmap(bundle_fd, file_offset, map_size,
                                           oasys::IO::MMAP_RO);
        if (bundle_ptr == NULL) {
            log_err( "Unable to map bundle file %s: %s", file_path.c_str(),
                     strerror(errno) );
            oasys::IO::close(bundle_fd);
            delete bundle;
            return;
        }
        
        // Feed data to BundleProtocol.
        int result = BundleProtocol::consume(bundle, (u_char*)bundle_ptr,
                                             map_size, &finished);
        
        // Unmap this chunk.
        if (oasys::IO::munmap(bundle_ptr, map_size) < 0) {
            log_err("Unable to unmap bundle file");
            oasys::IO::close(bundle_fd);
            delete bundle;
            return;
        }
        
        // Check the result of consume().
        if (result < 0) {
            log_err("Unable to process bundle");
            oasys::IO::close(bundle_fd);
            delete bundle;
            return;
        }
        
        // Update the file offset.
        file_offset += map_size;
    }
    
    // Close the bundle file and then delete it.
    oasys::IO::close(bundle_fd);
    if (::remove( file_path.c_str() ) < 0) {
        log_err( "Unable to remove bundle file %s: %s", file_path.c_str(),
                 strerror(errno) );
    }
     
    // If we didn't have a whole bundle, forget about it and bail out.
    if (!finished) {
        log_err( "Incomplete bundle in file %s", file_path.c_str() );
        delete bundle;
        return;
    }
    
    // If there are unused bytes in the file, log a warning, but
    // continue anyway.
    if (file_offset < (size_t)file_stat.st_size) {
        log_warn("Used only %zd of %zd bytes for the bundle", file_offset,
                 (size_t)file_stat.st_size);
    }
    
    if (message.bytes_received() != file_stat.st_size) {
        log_warn("bytes_received (%zd) does not match file size (%zd)",
                 (size_t)message.bytes_received(), (size_t)file_stat.st_size);
    } 
    
    // Tell the BundleDaemon about this bundle.
    BundleReceivedEvent* b_event =
            new BundleReceivedEvent(bundle, EVENTSRC_PEER, file_stat.st_size,
                                    NULL);
    BundleDaemon::post(b_event);
}

void
ECLModule::read_cycle() {
    size_t buffer_i = 0;

    // Peek at what's available.
    int result = socket_.recv(read_buffer_, READ_BUFFER_SIZE, MSG_PEEK);

    if (result <= 0) {
        log_err("Connection to CL %s lost: %s", name_.c_str(),
                (result == 0 ? "Closed by other side" : strerror(errno)));

        set_should_stop();
        return;
    } // if

    // Reserve enough room in the message buffer for this chunk and
    // a null terminating character.
    if (msg_buffer_.capacity() < msg_buffer_.size() + (size_t)result + 1)
        msg_buffer_.reserve(msg_buffer_.size() + (size_t)result + 1);

    // Push bytes onto the message buffer until we see the document root
    // closing tag (</dtn>) or run out of bytes.
    while (buffer_i < (size_t)result) {
        msg_buffer_.push_back(read_buffer_[buffer_i++]);

        // Check for the document root closing tag.
        if (msg_buffer_.size() > 12 && 
        strncmp(&msg_buffer_[msg_buffer_.size() - 13], "</cl_message>", 13) == 0) {
            // If we found the closing tag, add the null terminator and
            // parse the document into a CLEvent.
            msg_buffer_.push_back('\0');
            process_cl_event(&msg_buffer_[0], parser_);
                
            msg_buffer_.clear();
            break;
        } // if
    } // while

    // Read to the end of the document for real (we just peeked earlier).
    socket_.recv(read_buffer_, buffer_i, 0);
}

int
ECLModule::send_message(const cl_message* message)
{
    xercesc::MemBufFormatTarget buf;

    try {
        // Create the message and dump it out to 'buf'.
        cl_message_(buf, *message, ExternalConvergenceLayer::namespace_map_,
                    "UTF-8", xml_schema::flags::dont_initialize);
    }
    
    catch (xml_schema::serialization& e) {
        xml_schema::errors::const_iterator err_i;
        for (err_i = e.errors().begin(); err_i != e.errors().end(); ++err_i)
            log_err( "XML serialize error: %s", err_i->message().c_str() );
        
        return 0;
    }
    
    catch (std::exception& e) {
        log_err( "XML serialize error: %s", e.what() );
        return 0;
    }
    
    std::string msg_string( (char*)buf.getRawBuffer(), buf.getLen() );
    log_debug_p("/dtn/cl/XML", "Sending message to module %s:\n%s",
                name_.c_str(), msg_string.c_str() );
    
    // Send the message out the socket.
    int err = socket_.send( (char*)buf.getRawBuffer(), buf.getLen(),
                            MSG_NOSIGNAL );

    if (err < 0) {
        log_err("Socket error: %s", strerror(err));
        log_err("Connection with CL %s lost", name_.c_str());

        return -1;
    }
    
    return 0;
}

int 
ECLModule::prepare_bundle_to_send(cl_message* message)
{
    bundle_send_request request = message->bundle_send_request().get();
    
    // Find the link that this is going on.
    ECLLinkResource* link_resource = get_link( request.link_name() );
    if (!link_resource) {
        log_err( "Got bundle_send_request for unknown link %s",
                 request.link_name().c_str() );
        return -1;
    }
    
    // Find the bundle on the outgoing bundle list.
    //OutgoingBundle* outgoing_bundle =
    BundleRef bundle = 
            link_resource->get_outgoing_bundle( request.bundleAttributes() );
    if ( !bundle.object() ) {
        log_err( "Got bundle_send_request for unknown bundle");
        return 0;
    }
    
    //Bundle* bundle = outgoing_bundle->bundle.object();
    
    // Grab the bundle blocks for this bundle on this link.
    BlockInfoVec* blocks =
            bundle->xmit_blocks_.find_blocks(link_resource->link);
    if (!blocks) {
        log_err( "Bundle id %d on link %s has no block vectors",
                 bundle->bundleid_, request.link_name().c_str() );
        return 0;
    }
    
    // Calculate the total length of this bundle.
    size_t total_length = BundleProtocol::total_length(blocks);
    
    // Figure out the absolute path to the file.
    std::string abs_path = bundle_out_path_ + "/" + request.location();
    
    // Create and open the file.
    int bundle_fd = oasys::IO::open(abs_path.c_str(), O_RDWR | O_CREAT | O_EXCL,
                                    0644);
    if (bundle_fd < 0) {
        log_err( "Unable to create bundle file %s: %s",
                 request.location().c_str(), strerror(errno) );
        return 0;
    }
    
    // "Truncate" (expand, really) the file to the size of the bundle.
    if (oasys::IO::truncate(bundle_fd, total_length) < 0) {
        log_err( "Unable to resize bundle file %s: %s",
                 request.location().c_str(), strerror(errno) );
        oasys::IO::close(bundle_fd);
        bundle_send_failed(link_resource, bundle.object(), true);
        return 0;
    }
    
    size_t offset = 0;
    bool done = false;
    
    while (offset < total_length) {
        // Calculate the size of the next chunk and map it.
        size_t map_size = std::min(total_length - offset, MAX_BUNDLE_IN_MEMORY);
        void* bundle_ptr = oasys::IO::mmap(bundle_fd, offset, map_size,
                                           oasys::IO::MMAP_RW);
        if (bundle_ptr == NULL) {
            log_err( "Unable to map output file %s: %s",
                     request.location().c_str(), strerror(errno) );
            oasys::IO::close(bundle_fd);
            bundle_send_failed(link_resource, bundle.object(), true);
            return -1;
        }
        
        // Feed the next piece of bundle through BundleProtocol.
        BundleProtocol::produce(bundle.object(), blocks, (u_char*)bundle_ptr,
                                offset, map_size, &done);
        
        // Unmap this chunk.
        oasys::IO::munmap(bundle_ptr, map_size);
        offset += map_size;
    } 
    
    oasys::IO::close(bundle_fd);
    
    // Send this event to the module.
    return send_message(message);
}

void
ECLModule::bundle_send_failed(ECLLinkResource* link_resource,
                              Bundle* bundle,
                              bool erase_from_list)
{
    ContactRef contact = link_resource->link->contact();
    
    // Take the bundle off of the outgoing bundles list.
    if (erase_from_list)
        link_resource->erase_outgoing_bundle(bundle);
    
    // Figure out the relative and absolute path to the file.
    oasys::StringBuffer filename_buf("bundle%d", bundle->bundleid_);

    // Delete the bundle file.
    ::remove( (bundle_out_path_ + "/" + filename_buf.c_str()).c_str() );
}

ECLInterfaceResource*
ECLModule::get_interface(const std::string& name) const
{
    oasys::ScopeLock(&iface_list_lock_, "get_interface");
    std::list<ECLInterfaceResource*>::const_iterator iface_i;

    for (iface_i = iface_list_.begin(); iface_i != iface_list_.end();
    ++iface_i) {
        if ( (*iface_i)->interface->name() == name)
            return *iface_i;
    }

    return NULL;
}

ECLLinkResource*
ECLModule::get_link(const std::string& name) const
{
    sem_wait(&link_list_sem_);
    
    // First, check on the normal link list.
    LinkHashMap::const_iterator link_i = link_list_.find(name);
    if ( link_i == link_list_.end() ) {
        sem_post(&link_list_sem_);
        return NULL;
    }
    
    sem_post(&link_list_sem_);
    return link_i->second;
}

bool
ECLModule::link_exists(const std::string& name) const
{
    sem_wait(&link_list_sem_);
    
    // First, check on the normal link list.
    LinkHashMap::const_iterator link_i = link_list_.find(name);
    if ( link_i == link_list_.end() ) {
        sem_post(&link_list_sem_);
        return false;
    }

    sem_post(&link_list_sem_);
    return true;
}

ECLLinkResource*
ECLModule::create_discovered_link(const std::string& peer_eid,
                                  const std::string& nexthop,
                                  const std::string& link_name)
{	
    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    
    //lock the contact manager so no one opens the link before we do
	oasys::ScopeLock l(cm->lock(), "ECLModule::create_discovered_link");    
    LinkRef link = cm->new_opportunistic_link(&cl_, nexthop,
                                              EndpointID(peer_eid),
                                              &link_name);
    l.unlock();
    
    if (link == NULL) {
        log_debug("ECLModule::create_discovered_link: "
                  "failed to create new opportunistic link");
        return NULL;
    }
    
    // Create the resource holder for this link.
    ECLLinkResource* resource =
            new ECLLinkResource(name_, NULL, this, link, true);
    link->set_cl_info(resource);
    link->set_state(Link::AVAILABLE);
    
    // Wait twice on the semaphore to actually lock it.
    sem_wait(&link_list_sem_);
    sem_wait(&link_list_sem_);
    
    // Add this link to our list of links.
    link_list_.insert( LinkHashMap::value_type(link_name.c_str(), resource) );
    
    // Unlock the semaphore.
    sem_post(&link_list_sem_);
    sem_post(&link_list_sem_);

    return resource;
}

void
ECLModule::cleanup() {
    LinkHashMap::const_iterator link_i;
    
    // First, let the BundleDaemon know that all of the links are closed.
    for (link_i = link_list_.begin(); link_i != link_list_.end();
    ++link_i) {
        ECLLinkResource* resource = link_i->second;
        
        oasys::ScopeLock res_lock(&resource->lock, "ECLModule::cleanup");
        resource->known_state = Link::CLOSED;
        
        // Only report the closing if the link is currently open (otherwise,
        // the bundle daemon nags).
        if (resource->link->state() == Link::OPEN) {
            BundleDaemon::post( new LinkStateChangeRequest(resource->link,
                                Link::CLOSED, ContactEvent::NO_INFO) );
        }
        
        // Get this link's outgoing bundles.
        BundleList& bundle_set = resource->get_bundle_set();
        oasys::ScopeLock bundle_lock(bundle_set.lock(), "ECLModule::cleanup");
        
        // For each outgoing bundle, call bundle_send_failed to clean the
        // bundle.
        BundleList::iterator bundle_i;
        for (bundle_i = bundle_set.begin(); bundle_i != bundle_set.end();
        ++bundle_i) {
            bundle_send_failed(resource, *bundle_i, false);
        }
        
        // Clear the list of bundles that we just canceled.
        bundle_set.clear();
    }
    
    // At this point, we know that there are no links or interfaces pointing
    // to this module, so no new messages will come in.
    cl_message* message;
    while ( message_queue_.try_pop(&message) )
        delete message;

    // Give our interfaces and non-temporary links back to the CL.
    cl_.give_resources(iface_list_);
    cl_.give_resources(link_list_);
    
    // Delete the module's incoming and outgoing bundle directories.
    ::remove( bundle_in_path_.c_str() );
    ::remove( bundle_out_path_.c_str() );
}

} // namespace dtn

#endif // XERCES_C_ENABLED

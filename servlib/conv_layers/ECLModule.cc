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

#include <oasys/io/NetUtils.h>
#include <oasys/io/FileUtils.h>
#include <oasys/io/TCPClient.h>
#include <oasys/io/IO.h>
#include <oasys/thread/Lock.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/serialize/XMLSerialize.h>

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
    Thread("/dtn/cl/module"),
    cl_(cl),
    iface_list_lock_("/dtn/cl/parts/iface_list_lock"),
    socket_(fd, remote_addr, remote_port, logpath_),
    event_queue_("/dtn/cl/parts/module"),
    parser_( true, cl.schema_.c_str() )
{
    name_ = "";
    append_i_ = 0;
    event_ = NULL;
    next_bundle_id_ = 0;
    
    oasys::StringBuffer buffer("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<dtn eid=\"%s\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            "xsi:noNamespaceSchemaLocation=\"%s\">",
            BundleDaemon::instance()->local_eid().c_str(), cl.schema_.c_str());

    sem_init(&link_list_sem_, 0, 2);
    xml_header_ = std::string( buffer.c_str() );
    
    
}

ECLModule::~ECLModule()
{
    while (event_queue_.size() > 0)
        delete event_queue_.pop_blocking();
}

void
ECLModule::run()
{
    struct pollfd pollfds[2];

    struct pollfd* event_poll = &pollfds[0];
    event_poll->fd = event_queue_.read_fd();
    event_poll->events = POLLIN;

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

        // Check for events on the queue and send the next one out if there
        // are any.
        if (event_poll->revents & POLLIN) {
            CLEvent* event;
            if ( event_queue_.try_pop(&event) ) {
                ASSERT(event != NULL);
                int result;
                
                // We need to handle bundle-send messages as a special case,
                // in order to get the bundle written to disk first.
                if (event->type() == CL_BUNDLE_SEND) {
                    CLBundleSendRequest* send_request =
                            dynamic_cast<CLBundleSendRequest*>(event);
                    ASSERT(send_request);
                    
                    result = prepare_bundle_to_send(send_request);
                }
                
                else
                    result = send_event(event);
                
                delete event;

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

void ECLModule::post_event(CLEvent* event)
{
    // The MsgQueue is synchronized, so no need for a mutex here.
    event_queue_.push_back(event);
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
ECLModule::handle_cl_up(CLUpEvent* event)
{
    name_ = event->name;
    logpathf( "/dtn/cl/%s", name_.c_str() );
    event_queue_.logpathf( "/dtn/cl/parts/%s/event_queue", name_.c_str() );
    socket_.logpathf( "/dtn/cl/parts/%s/socket", name_.c_str() );
    log_info("New external CL: %s", name_.c_str());

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
    
    // Send the new CLA a reply.
    CLUpReplyEvent* up_reply = new CLUpReplyEvent(BundleDaemon::instance()->
            local_eid().c_str(), bundle_in_path_, bundle_out_path_);
    post_event(up_reply);
    
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

        CLInterfaceCreateRequest* msg =
                dynamic_cast<CLInterfaceCreateRequest*>(iface->create_message);
        ASSERT(msg);
        log_info( "Module %s acquiring interface %s", name_.c_str(),
                  iface->interface->name().c_str() );
        CLInterfaceCreateRequest* req =
                new CLInterfaceCreateRequest(*msg);
        post_event(req);
    }

    // Handle a Link.
    else if ( typeid(*resource) == typeid(ECLLinkResource) ) {
        ECLLinkResource* link = (ECLLinkResource*)resource;
            
        sem_wait(&link_list_sem_);
        sem_wait(&link_list_sem_);
        
        normal_links_.insert( LinkHashMap::value_type(link->link->name_str(),
                              link) );
        
        sem_post(&link_list_sem_);
        sem_post(&link_list_sem_);

        CLLinkCreateRequest* msg =
                dynamic_cast<CLLinkCreateRequest*>(link->create_message);
        ASSERT(msg);
        log_info( "Module %s acquiring link %s", name_.c_str(),
                  link->link->name() );
        CLLinkCreateRequest* req = new CLLinkCreateRequest(*msg);
        post_event(req);
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
ECLModule::handle_cl_interface_created(CLInterfaceCreatedEvent* event)
{
    ECLInterfaceResource* resource = get_interface(event->name);

    if (!resource) {
        log_info("Got %s for unknown interface %s", event->event_name().c_str(),
                 event->name.c_str());
        return;
    }
}

void
ECLModule::handle_cl_link_created(CLLinkCreatedEvent* event)
{
    ECLLinkResource* resource;

    if (event->discovered) {
        resource = create_discovered_link(event->peerEID, event->nextHop,
                                          event->name);
    }

    else {
        resource = get_link(event->name);
        
        // If we didn't find this link, check with ExternalConvergenceLayer
        // for anything recently added.
        if (!resource) {
            log_debug( "Got %s for unknown link %s, checking for new resources",
                       event->event_name().c_str(), event->name.c_str() );

            take_resources();
            resource = get_link(event->name);
            
            if (!resource) {
                log_err( "Got %s for unknown link %s", event->event_name().c_str(),
                        event->name.c_str() );
                return;
            } // if
        } // if
    } // else
}

void
ECLModule::handle_cl_link_state_changed(CLLinkStateChangedEvent* event)
{
    ECLLinkResource* resource = get_link(event->name);

    if (!resource) {
        log_err( "Got %s for unknown link %s", event->event_name().c_str(),
                 event->name.c_str() );
        return;
    }

    // When a discovered (temporary) link is closed, it can never come back,
    // so we need to remove it from the ContactManager and erase the link
    // holder. 
    if (resource->is_discovered && event->newState == Link::CLOSED) {
        log_info( "Deleting temporary link %s", resource->link->name() );
        oasys::ScopeLock(&cl_.global_resource_lock_,
                         "ECLModule::handle_cl_link_state_changed");
        
        sem_wait(&link_list_sem_);
        sem_wait(&link_list_sem_);
        
        temp_links_.erase( resource->link->name() );
        
        sem_post(&link_list_sem_);
        sem_post(&link_list_sem_);
        
        ContactManager* cm = BundleDaemon::instance()->contactmgr();
        if ( !BundleDaemon::instance()->shutting_down() )
            cm->del_link(resource->link);
            
        delete resource;
    }

    // Things get messy when bringing links up
    else if (event->newState == Link::OPEN) {
        if (resource->known_state == Link::OPEN)
            return;
        
        if (resource->link->state() != Link::OPENING)
            resource->link->set_state(Link::OPENING);
        
        resource->known_state = Link::OPEN;        
        
        ContactRef contact = resource->link->contact();
        if (contact == NULL) {
            contact = new Contact(resource->link);
            resource->link->set_contact( contact.object() );
        }
        
        BundleDaemon::post( new ContactUpEvent(contact) );
    }

    else if (resource->known_state != event->newState) {
        resource->known_state = event->newState;
        BundleDaemon::post( new LinkStateChangeRequest(resource->link,
                            event->newState, event->reason) );
    }
}

void
ECLModule::handle_cl_bundle_transmitted(CLBundleTransmittedEvent* event)
{
    // Find the link that this bundle was going to.
    ECLLinkResource* resource = get_link(event->linkName);
    if (!resource) {
        log_err( "Got %s for unknown link %s", event->event_name().c_str(),
                 event->linkName.c_str() );
        return;
    }

    OutgoingBundle* outgoing_bundle =
            resource->get_outgoing_bundle(event->bundleID);
    if (!outgoing_bundle) {
        log_err("Got %s for unknown bundle id %d", event->event_name().c_str(),
                 event->bundleID);
        return;
    }
    
    // Delete the bundle file.
    ::remove( outgoing_bundle->path.c_str() );

    // Tell the BundleDaemon about this.
    BundleTransmittedEvent* b_event =
            new BundleTransmittedEvent(outgoing_bundle->bundle.object(),
                                       outgoing_bundle->contact,
                                       resource->link,
                                       event->bytesSent,
                                       event->reliablySent);
    BundleDaemon::post(b_event);    
    resource->erase_outgoing_bundle(outgoing_bundle);
}

void
ECLModule::handle_cl_bundle_transmit_failed(CLBundleTransmitFailedEvent* event)
{
    // Find the link that this bundle was going to.
    ECLLinkResource* resource = get_link(event->linkName);
    if (!resource) {
        log_err( "Got %s for unknown link %s", event->event_name().c_str(),
                 event->linkName.c_str() );
        return;
    }

    OutgoingBundle* outgoing_bundle =
            resource->get_outgoing_bundle(event->bundleID);
    if (!outgoing_bundle) {
        log_err("Got %s for unknown bundle id %d", event->event_name().c_str(),
                event->bundleID);
        return;
    }
    
    bundle_send_failed(resource, outgoing_bundle, true);
}

void
ECLModule::handle_cl_bundle_received(CLBundleReceivedEvent* event)
{
    int bundle_fd;
    bool finished = false;
    size_t file_offset = 0;
    struct stat file_stat;
    std::string file_path = bundle_in_path_ + "/" + event->payloadFile;
    
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
        oasys::IO::munmap(bundle_ptr, map_size);
        
        if (result < 0) {
            log_err("Unable to process bundle");
            oasys::IO::close(bundle_fd);
            delete bundle;
            return;
        }
    }
    
    // Close the bundle file and then delete it.
    oasys::IO::close(bundle_fd);
    if (oasys::IO::unlink( file_path.c_str() ) < 0) {
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
    
    // Tell the BundleDaemon about this bundle.
    BundleReceivedEvent* b_event =
            new BundleReceivedEvent(bundle, EVENTSRC_PEER, event->bytesReceived,
                                    NULL);
    BundleDaemon::post(b_event);
}

void
ECLModule::read_cycle() {
    // If we have not read an entire xml document yet, keep working on it.
    if (!event_) {
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
            if (msg_buffer_.size() > 5 &&
            strncmp(&msg_buffer_[msg_buffer_.size() - 6], "</dtn>", 6) == 0) {
                // If we found the closing tag, add the null terminator and
                // parse the document into a CLEvent.
                msg_buffer_.push_back('\0');
                event_ = process_cl_event(&msg_buffer_[0], parser_);
                clear_parser(parser_);

                // If the parse succeeded and the document has appended data,
                // allocate space it, but don't put anything in to it yet.
                if (event_ && event_->appendedLength > 0)
                    event_->appendedData = new u_char[event_->appendedLength];

                break;
            } // if
        }

        // Read to the end of the document for real (we just peeked earlier).
        socket_.recv(read_buffer_, buffer_i, 0);
    }

    // If we have a CLEvent parsed and it expects appended data, keep reading.
    if (event_ && append_i_ < event_->appendedLength) {
        // Read to the tail of the appended data buffer without blocking.
        int result = socket_.recv( (char*)event_->appendedData + append_i_,
                        event_->appendedLength - append_i_, MSG_DONTWAIT);

        // If an error occurred that wasn't "We would block", something has
        // gone terribly wrong.
        if (result <= 0 && result != oasys::IOAGAIN) {
            log_err("Connection to CL %s lost: %s", name_.c_str(),
                    result == 0 ? "Closed by other side" : strerror(errno));
            set_should_stop();

            return;
        } // if

        // If no error and some data was read, update the end-of-buffer index.
        else if (result != oasys::IOAGAIN)
            append_i_ += result;
    } // if

    // If we have the whole document and all appended data (if any), dispatch
    // this message and clean it up.
    if (event_ && append_i_ == event_->appendedLength) {
        dispatch_cl_event(event_);
        delete event_;

        msg_buffer_.clear();
        append_i_ = 0;
        event_ = NULL;
    } // if
}

int
ECLModule::send_event(CLEvent* event)
{
    oasys::StringBuffer event_buf;
    event_buf.append( xml_header_.c_str() );

    oasys::XMLMarshal event_a( event_buf.expandable_buf(),
                               event->event_name().c_str() );
    event_a.action(event);
    
    log_debug("Sending message:\n%s", event_buf.c_str() + xml_header_.length());
    
    event_buf.append("</dtn>");

    int err = socket_.send((char*)event_buf.c_str(),
                            event_buf.length(), MSG_NOSIGNAL);

    if (err < 0) {
        log_err("Socket error: %s", strerror(err));
        log_err("Connection with CL %s lost", name_.c_str());

        return -1;
    }

    if (event->appendedLength > 0) {
        int err = socket_.send((char*)event->appendedData,
                               event->appendedLength, MSG_NOSIGNAL);

        if (err < 0) {
            log_err("Socket error: %s", strerror(err));
            log_err("Connection with CL %s lost", name_.c_str());
            return -1;
        }
    }

    return 0;
}

int 
ECLModule::prepare_bundle_to_send(CLBundleSendRequest* send_request)
{
    // Find the link that this is going on.
    ECLLinkResource* link_resource = get_link(send_request->linkName);
    if (!link_resource) {
        log_err( "Got %s for unknown link %s",
                 send_request->event_name().c_str(),
                 send_request->linkName.c_str() );
        return -1;
    }

    // Find the bundle on the outgoing bundle list.
    OutgoingBundle* outgoing_bundle = 
            link_resource->get_outgoing_bundle(send_request->bundleID);
    if (!outgoing_bundle) {
        log_err("Got %s for unknown bundle id %d",
                send_request->event_name().c_str(), send_request->bundleID);
        return -1;
    }
    
    Bundle* bundle = outgoing_bundle->bundle.object();
    
    // Grab the bundle blocks for this bundle on this link.
    BlockInfoVec* blocks =
            bundle->xmit_blocks_.find_blocks(link_resource->link);
    if (!blocks) {
        log_err( "Bundle id %d on link %s has no block vectors",
                 send_request->bundleID, send_request->linkName.c_str() );
        return -1;
    }
    
    // Calculate the total length of this bundle.
    size_t total_length = BundleProtocol::total_length(blocks);
    
    // Figure out the relative and absolute path to the file.
    oasys::StringBuffer filename_buf("bundle%d", next_bundle_id_++);
    std::string filename = std::string( filename_buf.c_str() );
    std::string abs_path = bundle_out_path_ + "/" + filename;
    
    // Create and open the file.
    int bundle_fd = oasys::IO::open(abs_path.c_str(), O_RDWR | O_CREAT | O_EXCL,
                                    0644);
    if (bundle_fd < 0) {
        log_err( "Unable to create bundle file %s: %s", filename.c_str(),
                 strerror(errno) );
        return -1;
    }
    
    // Save this bundle's path in case we need to erase it later. 
    outgoing_bundle->path = abs_path;
    
    // "Truncate" (expand, really) the file to the size of the bundle.
    if (oasys::IO::truncate(bundle_fd, total_length) < 0) {
        log_err( "Unable to resize bundle file %s: %s", filename.c_str(),
                 strerror(errno) );
        oasys::IO::close(bundle_fd);
        bundle_send_failed(link_resource, outgoing_bundle, true);
        return -1;
    }
    
    size_t offset = 0;
    bool done = false;
    
    while (offset < total_length) {
        // Calculate the size of the next chunk and map it.
        size_t map_size = std::min(total_length - offset, MAX_BUNDLE_IN_MEMORY);
        void* bundle_ptr = oasys::IO::mmap(bundle_fd, offset, map_size,
                                           oasys::IO::MMAP_RW);
        if (bundle_ptr == NULL) {
            log_err( "Unable to map output file %s: %s", filename.c_str(),
                     strerror(errno) );
            oasys::IO::close(bundle_fd);
            bundle_send_failed(link_resource, outgoing_bundle, true);
            return -1;
        }
        
        // Feed the next piece of bundle through BundleProtocol.
        BundleProtocol::produce(bundle, blocks, (u_char*)bundle_ptr, offset,
                                map_size, &done);
        
        // Unmap this chunk.
        oasys::IO::munmap(bundle_ptr, map_size);
        offset += map_size;
    } 
    
    oasys::IO::close(bundle_fd);

    // Send this event to the module.
    send_request->filename = std::string( filename.c_str() );
    return send_event(send_request);
}

void
ECLModule::bundle_send_failed(ECLLinkResource* link_resource,
                              OutgoingBundle* outgoing_bundle,
                              bool erase_from_list)
{
    BundleTransmitFailedEvent* event =
            new BundleTransmitFailedEvent(outgoing_bundle->bundle.object(), 
                                          outgoing_bundle->contact,
                                          link_resource->link);
 
    // Take the bundle off of the outgoing bundles list.
    if (erase_from_list)
        link_resource->erase_outgoing_bundle(outgoing_bundle);
    
    // Delete the bundle file.
    ::remove( outgoing_bundle->path.c_str() );
    
    // Tell the BundleDaemon about this bundle.
    BundleDaemon::post(event);
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
    LinkHashMap::const_iterator link_i = normal_links_.find(name);
    if ( link_i == normal_links_.end() ) {
        link_i = temp_links_.find(name);
        
        // If that fails, try the temp link list.
        if ( link_i == temp_links_.end() ) {
            sem_post(&link_list_sem_);
            return NULL;
        }
    }
    
    sem_post(&link_list_sem_);
    return link_i->second;
}

bool
ECLModule::link_exists(const std::string& name) const
{
    sem_wait(&link_list_sem_);
    
    // First, check on the normal link list.
    LinkHashMap::const_iterator link_i = normal_links_.find(name);
    if ( link_i == normal_links_.end() ) {
        // If that fails, try the temp link list.
        link_i = temp_links_.find(name);
        
        if ( link_i == temp_links_.end() ) {
            sem_post(&link_list_sem_);
            return false;
        }
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
    Link* link = cm->new_opportunistic_link(&cl_, nexthop, EndpointID(peer_eid),
                                            &link_name);

    // Create the resource holder for this link.
    ECLLinkResource* resource =
            new ECLLinkResource(name_, NULL, this, link, true);
    link->set_cl_info(resource);
    
    // Create this link's contact.
    Contact* contact = new Contact(link);
    link->set_contact(contact);

    sem_wait(&link_list_sem_);
    sem_wait(&link_list_sem_);
    
    // Add this link to our list of temporary links.
    temp_links_.insert( LinkHashMap::value_type(link_name.c_str(), resource) );
    
    sem_post(&link_list_sem_);
    sem_post(&link_list_sem_);

    return resource;
}

void
ECLModule::cleanup() {
    LinkHashMap::const_iterator link_i;
    
    // First, let the BundleDaemon know that all of the non-temporary links
    // are closed.
    for (link_i = normal_links_.begin(); link_i != normal_links_.end();
    ++link_i) {
        ECLLinkResource* resource = link_i->second;
        
        resource->known_state = Link::CLOSED;
        
        // Only report the closing if the link is currently open (otherwise,
        // the bundle daemon nags).
        if (resource->link->state() == Link::OPEN) {
            BundleDaemon::post( new LinkStateChangeRequest(resource->link,
                                Link::CLOSED, ContactEvent::NO_INFO) );
        }
        
        // Get this link's outgoing bundles.
        OutgoingBundleSet& bundle_set = resource->get_bundle_set();
        
        // For each outgoing bundle, call bundle_send_failed to clean the
        // bundle up and let the BundleDaemon know it is canceled.
        OutgoingBundleSet::iterator bundle_i;
        for (bundle_i = bundle_set.begin(); bundle_i != bundle_set.end();
        ++bundle_i) {
            bundle_send_failed(resource, &bundle_i->second, false);
        }
        
        // Clear the list of bundles that we just canceled.
        bundle_set.clear();
    }

    ContactManager* cm = BundleDaemon::instance()->contactmgr();

    // Now delete all of the temporary links.
    for (link_i = temp_links_.begin(); link_i != temp_links_.end();
    ++link_i) {
        ECLLinkResource* resource = link_i->second;
        
        // Set the known state to CLOSED because cm->del_link() will come back
        // around and call close_contact(). We don't want to work there with
        // a resource that we already deleted.
        resource->known_state = Link::CLOSED;
        resource->link->set_cl_info(NULL);

        // Tell the contact manager to delete this link and then delete our
        // reference to it.
        cm->del_link(resource->link);
        
        // Get this link's outgoing bundles.
        OutgoingBundleSet& bundle_set = resource->get_bundle_set();
        
        // For each outgoing bundle, call bundle_send_failed to clean the
        // bundle up and let the BundleDaemon know it is canceled.
        OutgoingBundleSet::iterator bundle_i;
        for (bundle_i = bundle_set.begin(); bundle_i != bundle_set.end();
        ++bundle_i) {
            bundle_send_failed(resource, &bundle_i->second, false);
        }

        delete resource;
    }
    
    // Clear the list of temporary links.
    temp_links_.clear();
    
    // Give our interfaces and non-temporary links back to the CL.
    cl_.give_resources(iface_list_);
    cl_.give_resources(normal_links_);
    
    // Delete the module's incoming and outgoing bundle directories.
    ::remove( bundle_in_path_.c_str() );
    ::remove( bundle_out_path_.c_str() );
}

} // namespace dtn

#endif // XERCES_C_ENABLED

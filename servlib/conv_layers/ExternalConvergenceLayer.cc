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

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <oasys/io/NetUtils.h>
#include <oasys/io/FileUtils.h>
#include <oasys/thread/Lock.h>
#include <oasys/util/OptParser.h>

#include "ExternalConvergenceLayer.h"
#include "ECLModule.h"
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"

namespace dtn {

bool ExternalConvergenceLayer::client_validation_ = true;
std::string ExternalConvergenceLayer::schema_ = "";
in_addr_t ExternalConvergenceLayer::server_addr_ = inet_addr("127.0.0.1");
u_int16_t ExternalConvergenceLayer::server_port_ = 5070;
xml_schema::namespace_infomap ExternalConvergenceLayer::namespace_map_;

ExternalConvergenceLayer::ExternalConvergenceLayer() :
ConvergenceLayer("ExternalConvergenceLayer", "extcl"),
global_resource_lock_("/dtn/cl/parts/global_resource_lock"),
module_mutex_("/dtn/cl/parts/module_mutex"),
resource_mutex_("/dtn/cl/parts/resource_mutex"),
listener_(*this)
{

}

ExternalConvergenceLayer::~ExternalConvergenceLayer()
{

}

void
ExternalConvergenceLayer::start()
{
    log_info("ExternalConvergenceLayer started");
    
    // Make sure that we were given a schema file to work with.
    if ( schema_ == std::string("") ) {
        log_err("No XML schema file specified."
                " ExternalConvergenceLayer is disabled.");
        return;
    }
    
    log_info( "Using XML schema file %s", schema_.c_str() );
    if (client_validation_)
        namespace_map_[""].schema = schema_.c_str();
    
    // Start the listener thread.
    listener_.start();
}

bool
ExternalConvergenceLayer::interface_up(Interface* iface, int argc,
                                       const char* argv[])
{
    const char* invalid;
    std::string proto_option;
    oasys::OptParser parser;

    // Find the protocol for this interface in the arguments.
    parser.addopt(new oasys::StringOpt("protocol", &proto_option));
    parser.parse(argc, argv, &invalid);
    if (proto_option.size() == 0) {
        log_err("Error parsing interface options: no 'protocol' option given");
        return false;
    }
    
    log_debug( "Adding interface %s for protocol %s", iface->name().c_str(),
               proto_option.c_str() );
    
    --argc;
    ++argv;
    
    // Turn the remaining arguments into a parameter sequence for XMLization.
    ParamSequence param_sequence;
    build_param_sequence(argc, argv, param_sequence);
    
    // Create the request message.
    interface_create_request request;
    request.interface_name( iface->name() );
    request.Parameter(param_sequence);
    
    cl_message* message = new cl_message();
    message->interface_create_request(request);
    
    ECLModule* module = get_module(proto_option);
    ECLInterfaceResource* resource =
            new ECLInterfaceResource(proto_option, message, module, iface);

    iface->set_cl_info(resource);
    
    oasys::ScopeLock l(&resource->lock, "interface_up");

    // Send the interface to the module if one was found for this protocol...
    if (module) {
        module->take_resource(resource);
    }

    // otherwise, add it to the unclaimed resource list.
    else {
        log_warn("No CL for protocol '%s' exists. "
                "Deferring initialization of interface %s",
                proto_option.c_str(), iface->name().c_str());

        add_resource(resource);
    }

    return true;
}

bool
ExternalConvergenceLayer::interface_down(Interface* iface)
{
    ECLInterfaceResource* resource =
            dynamic_cast<ECLInterfaceResource*>( iface->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock, "interface_down");

    // Remove this interface from the module's list and delete the resource.
    resource->module->remove_interface( iface->name() );
    delete resource;

    return true;
}

void
ExternalConvergenceLayer::dump_interface(Interface* iface,
                                         oasys::StringBuffer* buf)
{
    ECLInterfaceResource* resource =
            dynamic_cast<ECLInterfaceResource*>( iface->cl_info() );

    buf->appendf( "\tProtocol: %s", resource->protocol.c_str() );
}

bool
ExternalConvergenceLayer::init_link(const LinkRef& link,
                                    int argc, const char* argv[])
{
    std::string proto_option;
    oasys::OptParser parser;
    const char* invalidp;
    
    // If these are true, this is probably a request coming from
    // ContactManager::new_opportunistic_link(), which means the link already
    // exists.
    if (argc == 0 && argv == NULL)
        return true;

    // Find the protocol for this link in the arguments.
    parser.addopt(new oasys::StringOpt("protocol", &proto_option));
    parser.parse(argc, argv, &invalidp);
    if (proto_option.size() == 0) {
        log_err("Error parsing link options: no 'protocol' option given");
        return false;
    }
    
    log_debug( "Adding link %s for protocol %s", link->name(),
               proto_option.c_str() );
    
    ECLModule* module = get_module(proto_option);
    
    if ( module && module->link_exists( link->name() ) ) {
        log_err( "Link %s already exists on CL %s", link->name(),
                 proto_option.c_str() );
        return false;
    }
    
    ++argv;
    --argc;
    
    // Turn the remaining arguments into a parameter sequence for XMLization.
    ParamSequence param_sequence;
    build_param_sequence(argc, argv, param_sequence);
    param_sequence.push_back( clmessage::Parameter( "next_hop",
                              link->nexthop() ) );
    
    // Create the request message.
    link_create_request request;
    request.link_name( link->name_str() );
    request.type( XMLConvert::convert_link_type( link->type() ) );
    request.peer_eid( link->remote_eid().str() );
    request.Parameter(param_sequence);
    
    cl_message* message = new cl_message();
    message->link_create_request(request);
    
    ECLLinkResource* resource =
            new ECLLinkResource(proto_option, message, module, link, false);
    
    oasys::ScopeLock l(&resource->lock, "init_link");
    
    link->set_cl_info(resource);
    //link->set_flag(Link::CREATE_PENDING);
    
    if (module) {
        module->take_resource(resource);
    }
    
    else {
        log_warn( "No CL with protocol '%s' exists. "
                  "Deferring initialization of link %s",
                  proto_option.c_str(), link->name() );
        add_resource(resource);
    }

    return true;
}

void
ExternalConvergenceLayer::delete_link(const LinkRef& link)
{
    log_debug("ExternalConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    /*if (link->isdeleted() || link->cl_info() == NULL) {
        log_warn("ExternalConverganceLayer::delete_link: "
                 "link %s already deleted", link->name());
        return;
    }*/
    
    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock, "delete_link");
    
    // Clear out this link's cl_info.
    link->set_cl_info(NULL);
    
    // If the link is unclaimed, just remove it from the resource list.
    // This also handles the case of a LinkDeletedEvent coming from the CLA
    // with out a corresponding LinkDeleteRequest from the user.
    // NOTE: The ContactManager posts a LinkDeletedEvent, so we do not need
    // another one.
    if (resource->module == NULL) {
        delete_resource(resource);
        return;
    }
    
    // Otherwise, send a message to the CLA to have it delete the link.
    link_delete_request request;
    request.link_name( link->name_str() );
    POST_MESSAGE(resource->module, link_delete_request, request); 
}

void
ExternalConvergenceLayer::dump_link(const LinkRef& link,
                                    oasys::StringBuffer* buf)
{
	// Ignore destroyed links
    if (link->cl_info() == NULL) {
        log_warn( "Can't dump link information for destroyed link %s",
                  link->name() );
        return; 
    }
	
    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );

    buf->appendf( "\tProtocol: %s", resource->protocol.c_str() );
}

bool
ExternalConvergenceLayer::open_contact(const ContactRef& contact)
{
    log_debug("ExternalConvergenceLayer::open_contact enter");
    oasys::ScopeLock(&global_resource_lock_, "open_contact");
    
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    
    /*if(link->isdeleted() || link->cl_info() == NULL) {
        log_debug("ExternalConvergenceLayer::open_contact: "
                  "cannot open contact on deleted link %s", link->name());
        return false; 
    }*/
    
    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    if (!resource) {
        log_err( "Cannot open contact on link that does not exist" );
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::CLOSED,
                                       ContactEvent::NO_INFO));
        return false;
    }
    
    oasys::ScopeLock l(&resource->lock, "open_contact");
    
    if (!resource->module) {
        log_err( "Cannot open contact on unclaimed link %s", link->name() );
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::CLOSED,
                                       ContactEvent::NO_INFO));
        return false;
    }
    
    if (resource->known_state != Link::OPEN) {
        link_open_request request;
        request.link_name( link->name_str() );
        POST_MESSAGE(resource->module, link_open_request, request); 
        return true;
    }
    
    return true;
}

bool
ExternalConvergenceLayer::close_contact(const ContactRef& contact)
{
    oasys::ScopeLock(&global_resource_lock_, "open_contact");
    
    LinkRef link = contact->link();
    
    // Ignore destroyed links
	if (link->cl_info() == NULL) {
		log_warn( "Ignoring close contact request for destroyed link %s",
                  link->name() );
		return true; 
	}
    
    ECLLinkResource* resource =
                     dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock, "close_contact");
    
    // This indicates that the closing originated in the ECLModule.
    if (resource->known_state == Link::CLOSED)
        return true;
    
    if (resource->module == NULL) {
        log_err( "close_contact called for unclaimed link %s", link->name() );        
        return true;
    }
    
    resource->known_state = Link::CLOSED;
                         
    // Send a link close request to the CLA.
    link_close_request request;
    request.link_name( link->name_str() );
    POST_MESSAGE(resource->module, link_close_request, request); 
    
    return true;
}

void
ExternalConvergenceLayer::send_bundle(const ContactRef& contact, Bundle* bundle)
{
    oasys::ScopeLock(&global_resource_lock_, "send_bundle");
    
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    
    /*if (link->isdeleted() || link->cl_info() == NULL) {
        log_debug("ExternalConvergenceLayer::send_bundle: "
                  "cannot send bundle on deleted link %s", link->name());
        BundleDaemon::post(
            new BundleTransmitFailedEvent(bundle, contact, link));
        return;
    }*/

    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock, "send_bundle");

    // Make sure that this link is claimed by a module before trying to send
    // something through it.
    if (!resource->module) {
        log_err("Cannot send bundle on nonexistent CL %s through link %s",
                resource->protocol.c_str(), link->name());
        BundleDaemon::post( new BundleTransmitFailedEvent(bundle, contact,
                            resource->link) );
        return;
    }
    
    log_debug( "Sending bundle %d on link %s", bundle->bundleid_,
               contact->link()->name() );
    
    // Figure out the relative and absolute path to the file.
    oasys::StringBuffer filename_buf("bundle%d", bundle->bundleid_);
    std::string filename = std::string( filename_buf.c_str() );
    
    // Create the request message.
    bundle_send_request request;
    request.location(filename);
    request.link_name( link->name_str() );
    
    // Create and fill in the bundle attributes for this bundle.
    bundleAttributes bundle_attribs;
    fill_bundle_attributes(bundle, bundle_attribs);
    request.bundleAttributes(bundle_attribs);
    
    // Pass the bundle and the message on to the ECLModule.
    resource->add_outgoing_bundle(bundle);
    POST_MESSAGE(resource->module, bundle_send_request, request);
}


bool
ExternalConvergenceLayer::cancel_bundle(const LinkRef& link,
                                        Bundle* bundle)
{
    oasys::ScopeLock(&global_resource_lock_, "cancel_bundle");
    
    /*if (link->isdeleted() || link->cl_info() == NULL) {
        log_debug("ExternalConvergenceLayer::cancel_bundle: "
                "Cannot cancel bundle on deleted link %s", link->name());
        return false;
    }*/

    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock, "cancel_bundle");

    // Make sure that this link is claimed by a module.
    if (!resource->module) {
        log_err("Cannot cancel bundle on nonexistent CL %s through link %s",
                resource->protocol.c_str(), link->name());
        return false;
    }
    
    log_info( "Cancelling bundle %d on link %s", bundle->bundleid_,
               link->name() );
    
    // Create the request message.
    bundle_cancel_request request;
    request.link_name( link->name_str() );
    
    // Create and fill in the bundle attributes for this bundle.
    bundleAttributes bundle_attribs;
    fill_bundle_attributes(bundle, bundle_attribs);
    request.bundleAttributes(bundle_attribs);
    
    POST_MESSAGE(resource->module, bundle_cancel_request, request);
    return true;
}

bool 
ExternalConvergenceLayer::is_queued(const LinkRef& link, Bundle* bundle)
{
    oasys::ScopeLock(&global_resource_lock_, "is_queued");
    
    // Ignore deleted links.
    /*if (link->isdeleted() || link->cl_info() == NULL)
        return false;*/

    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);
    
    oasys::ScopeLock l(&resource->lock, "is_queued");
    
    // Check with the resource to see if the bundle is queued on it.
    return resource->has_outgoing_bundle(bundle);
}

void
ExternalConvergenceLayer::add_module(ECLModule* module)
{
    oasys::ScopeLock lock(&module_mutex_, "add_module");
    module_list_.push_back(module);
}

void
ExternalConvergenceLayer::remove_module(ECLModule* module)
{
    oasys::ScopeLock lock(&module_mutex_, "remove_module");
    std::list<ECLModule*>::iterator list_i = module_list_.begin();

    while ( list_i != module_list_.end() ) {
        if (*list_i == module) {
            module_list_.erase(list_i);
            return;
        }

        ++list_i;
    }
}

ECLModule*
ExternalConvergenceLayer::get_module(const std::string& name)
{
    std::string proto_option;
    std::list<ECLModule*>::iterator list_i;

    oasys::ScopeLock lock(&module_mutex_, "get_module");

    for (list_i = module_list_.begin(); list_i != module_list_.end();
    ++list_i) {
        if ((*list_i)->name() == name)
            return *list_i;
    }

    return NULL;
}

void
ExternalConvergenceLayer::add_resource(ECLResource* resource)
{
    oasys::ScopeLock lock(&resource_mutex_, "add_interface");
    resource_list_.push_back(resource);
}

void
ExternalConvergenceLayer::build_param_sequence(int argc, const char* argv[],
        ParamSequence& param_sequence)
{
    for (int arg_i = 0; arg_i < argc; ++arg_i) {
        std::string arg_string(argv[arg_i]);
        
        // Split the string in two around the '='.
        unsigned index = arg_string.find('=');
        if (index == std::string::npos) {
            log_warn("Invalid parameter: %s", argv[arg_i]);
            continue;
        }
        
        // Create a Parameter object from the two sides of the string.
        std::string lhs = arg_string.substr(0, index);
        std::string rhs = arg_string.substr(index + 1);
        param_sequence.push_back( clmessage::Parameter(lhs, rhs) );
    }
}

void
ExternalConvergenceLayer::fill_bundle_attributes(const Bundle* bundle,
                                                 bundleAttributes& attribs)
{
    attribs.source_eid( bundle->source_.str() );
    attribs.is_fragment(bundle->is_fragment_);
    
    // The timestamp in the XML element is a single 'long', rather than the
    // struct timeval used in DTN2.
    bundleAttributes::timestamp::type ts;
    ts = (bundleAttributes::timestamp::type)bundle->creation_ts_.seconds_ << 32
            | bundle->creation_ts_.seqno_;
    attribs.timestamp(ts);
    
    // fragment_offset and fragment_length are required only if is_fragment
    // is true.
    if (bundle->is_fragment_) {
        attribs.fragment_offset(bundle->frag_offset_);
        attribs.fragment_length(bundle->orig_length_);
    }
}

std::list<ECLResource*>
ExternalConvergenceLayer::take_resources(std::string name, ECLModule* owner)
{
    oasys::ScopeLock lock(&resource_mutex_, "take_resources");
    std::list<ECLResource*> new_list;
    std::list<ECLResource*>::iterator list_i = resource_list_.begin();

    while ( list_i != resource_list_.end() ) {
        if ( (*list_i)->protocol == name ) {
            // Set the resource's new owner and add it to the new list.
            (*list_i)->module = owner;
            new_list.push_back(*list_i);
            
            // Erase this resource from the unclaimed list.
            std::list<ECLResource*>::iterator erase_i = list_i;
            ++list_i;
            resource_list_.erase(erase_i);
        }

        else {
            ++list_i;
        }
    }

    return new_list;
}

void
ExternalConvergenceLayer::delete_resource(ECLResource* resource)
{
    oasys::ScopeLock lock(&resource_mutex_, "delete_resource");
    resource_list_.remove(resource);
    delete resource;
}    

void
ExternalConvergenceLayer::give_resources(std::list<ECLInterfaceResource*>& list)
{
    std::list<ECLInterfaceResource*>::iterator list_i;

    resource_mutex_.lock("give_resources(ECLInterfaceResource)");
    for (list_i = list.begin(); list_i != list.end(); ++list_i) {
        (*list_i)->module = NULL;
        resource_list_.push_back( (*list_i) );
    }

    resource_mutex_.unlock();

    log_debug( "Got %d ECLInterfaceResources", list.size() );
    list.clear();
}

void
ExternalConvergenceLayer::give_resources(LinkHashMap& list)
{
    LinkHashMap::iterator list_i;

    resource_mutex_.lock("give_resources(ECLLinkResource)");
    for (list_i = list.begin(); list_i != list.end(); ++list_i) {
        list_i->second->module = NULL;
        resource_list_.push_back(list_i->second);
    }

    resource_mutex_.unlock();

    log_debug( "Got %d ECLLinkResources", list.size() );
    list.clear();
}

ExternalConvergenceLayer::Listener::Listener(ExternalConvergenceLayer& cl)
    : IOHandlerBase(new oasys::Notifier("/dtn/cl/Listener")),
      TCPServerThread("/dtn/cl/Listener"),
      cl_(cl)
{
    Thread::set_flag(Thread::DELETE_ON_EXIT);
    set_logpath("/dtn/cl/Listener");
    set_logfd(false);
}

ExternalConvergenceLayer::Listener::~Listener()
{

}

void
ExternalConvergenceLayer::Listener::start()
{
    log_info("Listening for external CLAs on %s:%d", intoa(server_addr_),
             server_port_);

    if (bind_listen_start(server_addr_, server_port_) < 0)
            log_err("Listener thread failed to start");
}

void
ExternalConvergenceLayer::Listener::accepted(int fd, in_addr_t addr,
                                             u_int16_t port)
{
    if ( schema_ == std::string() ) {
        log_err("ECLA module is connecting before the XSD file is specified"
                "Closing the socket");
        ::close(fd);
        return;
    }
    
    ECLModule* module = new ECLModule(fd, addr, port, cl_);

    // Add this module to our list and start its thread.
    cl_.add_module(module);
    module->start();
}


ECLLinkResource::ECLLinkResource(std::string p, clmessage::cl_message* create,
                                 ECLModule* m, const LinkRef& l, bool disc) :
    ECLResource(p, create, m),
    link(l.object(), "ECLLinkResource"),
    outgoing_bundles_("outgoing_bundles")
{
    known_state = Link::UNAVAILABLE;
    is_discovered = disc;
}

void
ECLLinkResource::add_outgoing_bundle(Bundle* bundle) {
    outgoing_bundles_.push_back(bundle);
}

BundleRef
ECLLinkResource::get_outgoing_bundle(clmessage::bundleAttributes bundle_attribs)
{
    EndpointID eid( bundle_attribs.source_eid() );
    BundleTimestamp timestamp;
    timestamp.seconds_ = bundle_attribs.timestamp() >> 32;
    timestamp.seqno_ = bundle_attribs.timestamp() & 0xffffffff;
    
    return outgoing_bundles_.find(eid, timestamp);
}

bool
ECLLinkResource::has_outgoing_bundle(Bundle* bundle)
{
    return outgoing_bundles_.contains(bundle);
}

void
ECLLinkResource::erase_outgoing_bundle(Bundle* bundle)
{
    outgoing_bundles_.erase(bundle);
}

BundleList&
ECLLinkResource::get_bundle_set() 
{
    return outgoing_bundles_;
}


clmessage::linkTypeType
XMLConvert::convert_link_type(Link::link_type_t type)
{
    switch (type) {
        case Link::ALWAYSON: return linkTypeType(linkTypeType::alwayson);
        case Link::ONDEMAND: return linkTypeType(linkTypeType::ondemand);
        case Link::SCHEDULED: return linkTypeType(linkTypeType::scheduled);
        case Link::OPPORTUNISTIC: 
            return linkTypeType(linkTypeType::opportunistic);
        default: break;
    }
    
    return linkTypeType();
}

Link::link_type_t
XMLConvert::convert_link_type(clmessage::linkTypeType type)
{
    switch (type) {
        case linkTypeType::alwayson: return Link::ALWAYSON;
        case linkTypeType::ondemand: return Link::ONDEMAND;
        case linkTypeType::scheduled: return Link::SCHEDULED;
        case linkTypeType::opportunistic: return Link::OPPORTUNISTIC;
        default: break;
    }
        
    return Link::LINK_INVALID;
}

Link::state_t 
XMLConvert::convert_link_state(clmessage::linkStateType state) 
{
    switch (state) {
        case linkStateType::unavailable: return Link::UNAVAILABLE;
        case linkStateType::available: return Link::AVAILABLE;
        case linkStateType::opening: return Link::OPENING;
        case linkStateType::open: return Link::OPEN;
        case linkStateType::busy: return Link::BUSY;
        case linkStateType::closed: return Link::CLOSED;
        default: break;
    }
    
    ASSERT(false);
    return Link::CLOSED;
}

ContactEvent::reason_t
XMLConvert::convert_link_reason(clmessage::linkReasonType reason)
{
    switch (reason) {
        case linkReasonType::no_info: return ContactEvent::NO_INFO;
        case linkReasonType::user: return ContactEvent::USER;
        case linkReasonType::broken: return ContactEvent::BROKEN;
        case linkReasonType::shutdown: return ContactEvent::SHUTDOWN;
        case linkReasonType::reconnect: return ContactEvent::RECONNECT;
        case linkReasonType::idle: return ContactEvent::IDLE;
        case linkReasonType::timeout: return ContactEvent::TIMEOUT;
        case linkReasonType::unblocked: return ContactEvent::UNBLOCKED;
        default: break;
    }
    
    return ContactEvent::INVALID;
}
    
} // namespace dtn

#endif // XERCES_C_ENABLED

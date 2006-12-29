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

namespace dtn {

std::string ExternalConvergenceLayer::schema_ = "";
in_addr_t ExternalConvergenceLayer::server_addr_ = inet_addr("127.0.0.1");
u_int16_t ExternalConvergenceLayer::server_port_ = 0;

ExternalConvergenceLayer::ExternalConvergenceLayer() :
ConvergenceLayer("ExternalConvergenceLayer", "extcl"),
global_resource_lock_("/dtn/cl/parts/global_resource_lock"),
module_mutex_("/dtn/cl/parts/module_mutex"),
resource_mutex_("/dtn/cl/parts/resource_mutex"),
listener_(*this)
{
    log_info("ExternalConvergenceLayer started");
}

ExternalConvergenceLayer::~ExternalConvergenceLayer()
{

}

void
ExternalConvergenceLayer::start()
{
    log_info( "Using XML schema file %s", schema_.c_str() );
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

    CLInterfaceCreateRequest* request =
            new CLInterfaceCreateRequest(iface->name(), argc, argv);

    ECLModule* module = get_module(proto_option);
    ECLInterfaceResource* resource =
            new ECLInterfaceResource(proto_option, request, module, iface);

    iface->set_cl_info(resource);

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

    // Tell the module to destroy this interface.
    resource->module->post_event(new CLInterfaceDestroyRequest(iface->name()));

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
ExternalConvergenceLayer::init_link(Link* link, int argc, const char* argv[])
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
    
    CLLinkCreateRequest* request =
            new CLLinkCreateRequest(link->type(), link->name_str(),
                                    link->remote_eid().str(),
                                    std::string( link->nexthop() ),
                                    argc - 1, argv + 1);

    ECLLinkResource* resource =
            new ECLLinkResource(proto_option, request, module, link, false);
    link->set_cl_info(resource);
    
    if (module) {
        if (link->type() == Link::OPPORTUNISTIC)
            link->set_state(Link::UNAVAILABLE);
    
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
ExternalConvergenceLayer::dump_link(Link* link, oasys::StringBuffer* buf)
{
    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );

    buf->appendf( "\tProtocol: %s", resource->protocol.c_str() );
}

bool
ExternalConvergenceLayer::open_contact(const ContactRef& contact)
{
    oasys::ScopeLock(&global_resource_lock_, "open_contact");
    
    Link* link = contact->link();
    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );

    if(!resource){
        log_err( "Cannot open contact on link that does not exist" );
        return false;
    }

    
    if (!resource->module) {
        log_err( "Cannot open contact on unclaimed link %s", link->name() );
        return false;
    }
    
    if (resource->known_state != Link::OPEN) {
        resource->module->post_event( new CLLinkOpenRequest( link->name_str() ) );
        return false;
    }
    
    return true;
}

bool
ExternalConvergenceLayer::close_contact(const ContactRef& contact)
{
    oasys::ScopeLock(&global_resource_lock_, "open_contact");
    
    Link* link = contact->link();
    ECLLinkResource* resource =
                     dynamic_cast<ECLLinkResource*>( link->cl_info() );
    
    // This indicates that the closing originated in the ECLModule. In that
    // case, resource->module has probably already been NULLed out.
    if (resource->known_state == Link::CLOSED)
        return true;
    
    if (resource->module == NULL) {
        log_err( "close_contact called for unclaimed link %s", link->name() );        
        return true;
    }
    
    resource->known_state = Link::CLOSED;                     
    resource->module->post_event( new CLLinkCloseRequest( link->name_str() ) );
    
    return true;
}

void
ExternalConvergenceLayer::send_bundle(const ContactRef& contact, Bundle* bundle)
{
    oasys::ScopeLock(&global_resource_lock_, "send_bundle");
    
    Link* link = contact->link();
    if ( !link->cl_info() ) {
        log_err("Link %s has no cl_info", link->name() );
        BundleDaemon::post( new BundleTransmitFailedEvent(bundle, contact,
                            contact->link() ) );
        return;
    }

    ECLLinkResource* resource =
            dynamic_cast<ECLLinkResource*>( link->cl_info() );
    ASSERT(resource);

    // Make sure that this link is claimed by a module before trying to send
    // something through it.
    if (!resource->module) {
        log_err("Cannot send bundle on nonexistent CL %s through link %s",
                resource->protocol.c_str(), link->name());
        BundleDaemon::post( new BundleTransmitFailedEvent(bundle, contact,
                            resource->link) );
        return;
    }
    
    log_debug( "Sending bundle on link %s", contact->link()->name() );

    CLBundleSendRequest* request = new CLBundleSendRequest( bundle->bundleid_,
            link->name(), std::string() );

    resource->add_outgoing_bundle(bundle);
    resource->module->post_event(request);
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


ECLLinkResource::ECLLinkResource(std::string p, CLLinkCreateRequest* create,
                                 ECLModule* m, Link* l, bool disc) :
    ECLResource(p, create, m),
    bundle_mutex_("/dtn/cl/parts/bundle_mutex")
{
    link = l;
    known_state = Link::UNAVAILABLE;
    is_discovered = disc;
}

void
ECLLinkResource::add_outgoing_bundle(Bundle* bundle) {
    oasys::ScopeLock lock(&bundle_mutex_, "add_outgoing_bundle");
    
    OutgoingBundle outgoing( bundle, link->contact().object() );    
    outgoing_bundles_.insert( OutgoingBundleSet::value_type(bundle->bundleid_,
                              outgoing) );
}

OutgoingBundle*
ECLLinkResource::get_outgoing_bundle(u_int32_t bundleid)
{
    oasys::ScopeLock lock(&bundle_mutex_, "get_outgoing_bundle");
    OutgoingBundleSet::iterator bundle_i =
            outgoing_bundles_.find(bundleid);
    
    if ( bundle_i == outgoing_bundles_.end() )
        return NULL;

    return &bundle_i->second;
}

void
ECLLinkResource::erase_outgoing_bundle(OutgoingBundle* outgoing_bundle)
{
    oasys::ScopeLock lock(&bundle_mutex_, "erase_outgoing_bundle");
    outgoing_bundles_.erase(outgoing_bundle->bundle->bundleid_);
}

OutgoingBundleSet&
ECLLinkResource::get_bundle_set() 
{
    return outgoing_bundles_;
}

} // namespace dtn

#endif // XERCES_C_ENABLED

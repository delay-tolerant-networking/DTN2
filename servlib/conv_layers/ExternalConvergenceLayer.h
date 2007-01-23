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

#ifndef _EXTERNAL_CONVERGENCE_LAYER_H_
#define _EXTERNAL_CONVERGENCE_LAYER_H_

#include <config.h>
#ifdef XERCES_C_ENABLED

#include <string>
#include <list>
#include <ext/hash_map>

#include <oasys/thread/Thread.h>
#include <oasys/thread/Mutex.h>
#include <oasys/io/TCPServer.h>

#include "ConvergenceLayer.h"
#include "clevent.h"
#include "bundling/BundleList.h"
#include "bundling/BundleEvent.h"

class dtn::clmessage::bundleAttributes;

namespace dtn {

using __gnu_cxx::hash_multimap;
using __gnu_cxx::hash;

class Interface;
class Contact;
class ECLModule;

typedef ::xsd::cxx::tree::sequence<clmessage::Parameter> ParamSequence;

/** Base class for resources that should be owned by an ECLModule.
 * 
 * Classes derived from this class are used for keeping track of various
 * resources (interfaces and links, for instance) that belong to a particular
 * module.  This base class holds info about the owner module as well as a copy
 * of the CLEvent that tells the external module to create the resource.
 */
class ECLResource : public CLInfo {
public:
    virtual ~ECLResource() {
        delete create_message;
    }
    
    /// The protocol that this resource is intended for.
    std::string protocol;
    
    /// The CLEvent that will create this resource, in case we must send
    /// it again.
    clmessage::cl_message* create_message;
    
    /// The module that owns this resource (this will be NULL if the resource
    /// is unclaimed).
    ECLModule* module;
    
    oasys::Mutex lock;
    
protected:
    ECLResource(std::string p, clmessage::cl_message* create, ECLModule* m) :
    lock("ECLResource") {
        protocol = p;
        module = m;
        create_message = create;
    }
};


/** Represents a single Interface.
 */
class ECLInterfaceResource : public ECLResource {
public:
    ECLInterfaceResource(std::string p, clmessage::cl_message* create,
    ECLModule* m, Interface* i) : ECLResource(p, create, m) {
        interface = i;
    }
    
    Interface* interface;
};


/** Represents a single Link.
 */
class ECLLinkResource : public ECLResource {
public:
    ECLLinkResource(std::string p, clmessage::cl_message* create, ECLModule* m,
                    const LinkRef& l, bool disc);
    
    /// Reference to the link that this Resource represents.
    LinkRef link;
    
    /// The state that the ECLModule knows the link to be in, independent of
    /// the state in the link itself. This is useful for state changes that
    /// originate at the CL rather than the BPA. 
    Link::state_t known_state;
    
    /// True if this link was discovered rather than created by the BPA.
    bool is_discovered;
    
    
    /** Add a bundle to this link's outgoing bundle list.
     *
     * @param bundle  The bundle to be placed on the outgoing list. A new
     *                BundleRef will be created for the bundle.
     */
    void add_outgoing_bundle(Bundle* bundle);
    
    
    /** Retrieve a bundle from this link's outgoing bundle list.
     *
     * @param bundleid  The ID of the bundle to retrieve.
     * 
     * @return The OutgoingBundle object for the requested bundle if it
     *         exists on the outgoing bundle list, NULL if it does not.
     */
    BundleRef get_outgoing_bundle(clmessage::bundleAttributes bundle_attribs);
    
    bool has_outgoing_bundle(Bundle* bundle);
    
    
    /** Erase a bundle from this link's outgoing bundle list.
     * 
     * @param outgoing_bundle  The bundle to be removed from the outgoing list.
     */
    void erase_outgoing_bundle(Bundle* bundle);
    
    
    /** Get the actual outgoing bundle list.
     * 
     * This is used in ECLModule::cleanup() to iterate through the list.
     */
    BundleList& get_bundle_set();
    
private:
    /// The list of bundles going out on this link.
    BundleList outgoing_bundles_;
};


/** Hash function for a std::string.
 */
struct StringHash {
    size_t operator()(std::string s) const {
        size_t h = 0;
        for (unsigned i = 0; i < s.length(); ++i)
            h = h * 5 + s[i];
        
        return h;
    }
};

typedef hash_multimap<std::string, ECLLinkResource*, StringHash>
        LinkHashMap;


/** The external convergence layer proxy on the DTN2 side.
 * 
 * This class interacts with DTN2 as any other conventional convergence layer.
 * All interfaces and links on an external CLA appear to DTN2 to be on this
 * CL.
 * 
 * Every Interface or Link intended for an external module must specify this
 * convergence layer's name ('extcl') as its convergence layer at startup.  A
 * parameter 'protocol={module name}' must appear before any CL-specific
 * parameters for the resource to indicate which external module it is for.
 * A list of any resources intended for an external module that has not yet
 * connected to DTN2 is maintained (this is known as the 'unclaimed resource
 * list').
 */
class ExternalConvergenceLayer : public ConvergenceLayer {
public:
    ExternalConvergenceLayer();
    ~ExternalConvergenceLayer();

    /** Start the ECLA listener thread.
     * 
     * This should be called before an instance of ExternalConvergenceLayer is
     * given to ConvergenceLayer.
     */
    void start();
    
    bool interface_up(Interface* iface, int argc, const char* argv[]);
    bool interface_down(Interface* iface);
    void dump_interface(Interface* iface, oasys::StringBuffer* buf);
    bool init_link(const LinkRef& link, int argc, const char* argv[]);
    void delete_link(const LinkRef& link);
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf);
    bool open_contact(const ContactRef& contact);
    bool close_contact(const ContactRef& contact);
    void send_bundle(const ContactRef& contact, Bundle* bundle);
    bool cancel_bundle(const LinkRef& link, Bundle* bundle);
    bool is_queued(const LinkRef& link, Bundle* bundle);

    
    /** Take unclaimed resources intended for a given protocol.
     * 
     * This will assign to the given module any resource on the unclaimed
     * resource list matching the given protocol by setting the
     * ECLResource::module field.  All matching resources are removed from the
     * unclaimed resource list and returned.
     * 
     * @param protocol - The name of the protocol to match.
     * @param owner - The module that will own the matching resources.
     * 
     * @return A list containing all resources that matched the protocol. 
     */
    std::list<ECLResource*> take_resources(std::string protocol, ECLModule* owner);
    
    
    /** Give a list of Interface resources back to the unclaimed resource list.
     * 
     * The resources will be placed back on the unclaimed resource list and
     * the ECLResource::module field set to NULL.
     */
    void give_resources(std::list<ECLInterfaceResource*>& list);
    
    
    /** Give a list of Interface resources back to the unclaimed resource list.
     * 
     * The resources will be placed back on the unclaimed resource list and
     * the ECLResource::module field set to NULL.
     */
    void give_resources(LinkHashMap& list);
    
    
    /** Add a module to the active module list.
     */
    void add_module(ECLModule* module);
    
    
    /** Remove a module from the active module list.
     */
    void remove_module(ECLModule* module);
    
    
    /** Retrieve a module matching the given protocol name.
     * 
     * @param protocol - The name of the protocol to match.
     * @return A pointer to the module matching the protocol, or NULL if no
     *      such module exists.
     */
    ECLModule* get_module(const std::string& protocol);
    
    /// The path to the XSD file that specifies the XML messages between the
    /// BPA and the CLA. This is set with the command 'ecla set xsd_file'
    static std::string schema_;
    
    static bool client_validation_;
    
    /// The address on which the Listener thread will listen. This is set with
    /// the command 'ecla set listen_addr'
    static in_addr_t server_addr_;
    
    /// The address on which the Listener thread will listen. This is set with
    /// the command 'ecla set listen_port'
    static u_int16_t server_port_;
    
    /// This is used in ECLModule::send_message() to generate the XML
    /// namespace/schema info at the start of every message.
    static xml_schema::namespace_infomap namespace_map_;

    
    /// ECLModule locks this when it enters ECLModule::cleanup() to prevent
    /// race conditions on resources that are being cleaned up.
    oasys::Mutex global_resource_lock_;
    
    
private:
    /** Thread to listen for connections from new external modules.
     * 
     * This thread will wait on accept() for connections on the designated
     * port (where this port is specified is TBD) on localhost for connections
     * from new external modules.  For each new connection, an ECLModule is
     * created and started.
     */
    class Listener : public oasys::TCPServerThread {
    public:
        Listener(ExternalConvergenceLayer& cl);
        virtual ~Listener();

        void start();
        virtual void accepted(int fd, in_addr_t addr, u_int16_t port);

    private:
        /// Reference back to the convergence layer.
        ExternalConvergenceLayer& cl_;
    }; // class Listener
    
    
    /** Add a resource to the unclaimed resource list.
     */
    void add_resource(ECLResource* resource);
    
    /** Delete a resource.
     * 
     * This will remove the resource from the list of unclaimed resources and
     * call 'delete' on the pointer.
     */
    void delete_resource(ECLResource* resource);

    
    /** Convert an arg vector to a ParamSequence.
     * 
     * This will take a number of strings (in argv) in the form 'name=value'
     * and convert them to a ParamSequence suitable for XML messages.
     * 
     * @param argc  Number of arguments in argv.
     * @param argv  The vector of arguments.
     * @param param_sequence  An instance of ParamSequence that will be
     *                        populated with the parameters.
     */
    void build_param_sequence(int argc, const char* argv[],
                              ParamSequence& param_sequence);
    
    /** Fill in the fields of a bundleAttributes object.
     * 
     * This will complete the bundleAttributes instance based on the given
     * bundle.
     */
    void fill_bundle_attributes(const Bundle* bundle,
                                clmessage::bundleAttributes& attribs);
        
    /// The list of active modules.
    std::list<ECLModule*> module_list_;
    
    /// Mutex for module_list_ access.
    oasys::Mutex module_mutex_;
    
    /// The unclaimed resource list.
    std::list<ECLResource*> resource_list_;
    
    /// Mutex for resource_list_ access.
    oasys::Mutex resource_mutex_;
    
    /// The thread listening for new modules.
    Listener listener_;
};


/** Functions for converting between various DTN2 types and their corresponding
 * value in the clmessage namespace.
 */
class XMLConvert {
public:
    static clmessage::linkTypeType convert_link_type(Link::link_type_t type);
    static Link::link_type_t convert_link_type(clmessage::linkTypeType type);
    
    static Link::state_t convert_link_state(clmessage::linkStateType state);
    
    static ContactEvent::reason_t convert_link_reason(
            clmessage::linkReasonType reason);
};

} // namespace dtn

#endif // XERCES_C_ENABLED
#endif // _EXTERNAL_CONVERGENCE_LAYER_H_


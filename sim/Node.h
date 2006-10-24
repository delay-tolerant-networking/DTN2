/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _NODE_H_
#define _NODE_H_

#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Log.h>

#include "SimEventHandler.h"
#include "bundling/BundleDaemon.h"

using namespace dtn;

namespace dtn {

class ContactManager;

}

namespace dtnsim {

/**
 * Class representing a node in the simulator (i.e. a router plus
 * associated links, etc).
 *
 * Derives from the core dtn BundleDaemon and whenever an event is
 * processed that relates to a node, that node is installed as the
 * BundleDaemon::instance().
 */
class Node : public SimEventHandler, public BundleDaemon {
public:
    /**
     * Constructor
     */
    Node(const char* name);

    /**
     * Virtual initialization function.
     */
    void do_init();

    /**
     * Destructor
     */
    virtual ~Node() {}
        
    /**
     * Virtual post function, overridden in the simulator to use the
     * modified event queue.
     */
    virtual void post_event(BundleEvent* event);
    
    /**
     * Virtual function from SimEventHandler
     */
    virtual void process(SimEvent *e);

    /**
     * Process all pending bundle events until the queue is empty.
     */
    void process_bundle_events();
    
    /**
     * Accessor for name.
     */
    const char* name() { return name_.c_str(); }
    
    /**
     * Accessor for router.
     */
    BundleRouter* router() { return router_; }

    /**
     * Set the node as the "active" node in the simulation. This
     * swings the static BundleDaemon::instance_ pointer to point to
     * the node so all singleton accesses throughout the code will
     * reference the correct node.
     *
     * It also sets the node name as the logging prefix in oasys.
     */
    void set_active()
    {
        instance_ = this;
        oasys::Log::instance()->set_prefix(name_.c_str());
    }

    /**
     * Return the current active node.
     */
    static Node* active_node()
    {
        return (Node*)instance_;
    }

    /**
     * Return the next available bundle id.
     */
    u_int32_t next_bundleid()
    {
        return next_bundleid_++;
    }

    /**
     * Return the next available registration id.
     */
    u_int32_t next_regid()
    {
        return next_regid_++;
    }
    
//     /**
//      * Action when informed that message transmission is finished.
//      * Invoked by SimContact
//      */
//     virtual  void chewing_complete(SimContact* c, double size, Message* msg) = 0;
    
//     /**
//      * Action when a contact opens/closes
//      * Invoked by SimContact
//      */
//     virtual  void open_contact(SimContact* c) = 0;
//     virtual  void close_contact(SimContact* c) = 0;

//     /**
//      * Action when the MESSAGE_RECEIVED event arrives
//      */
//     virtual  void message_received(Message* msg) = 0;
    
//     virtual  void create_consumer() {};
    
protected:
    const std::string   name_;
    u_int32_t		next_bundleid_;
    u_int32_t		next_regid_;
    std::queue<BundleEvent*>* eventq_;
};

} // namespace dtnsim

#endif /* _NODE_H_ */

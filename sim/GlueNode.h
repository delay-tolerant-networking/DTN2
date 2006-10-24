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

#ifndef _GLUE_NODE_H_
#define _GLUE_NODE_H_

/*
 * Node which interfaces to DTN2  and Simulator.
 * This node is responsible for forwarding/converting events
 * from Simulator to DTN2 and vice-versa.
 * It acts as  BundleDaemon. On receiving bundles/contact dynamics
 * etc... it informs the BundleRouter and  executes the returned
 * list of action. The actual forwarding of a Bundle using the
 * Simulator is done at the SimulatorConvergence layer.
 */


#include "Node.h"
#include "routing/BundleRouter.h"
#include "FloodConsumer.h"

namespace dtnsim {

class GlueNode : public Node {

public:


    GlueNode(int id, const char* logpath);
    /**
     * Virtual functions from Node
     */
    virtual void process(Event *e); ///< virtual function from Processable
    virtual  void chewing_complete(SimContact* c, double size, Message* msg);
    virtual  void open_contact(SimContact* c);
    virtual  void close_contact(SimContact* c);
    virtual  void message_received(Message* msg);
    
    virtual  void create_consumer();
    
private:

    /**
     * Forward the message to next hop. 
     * Basically, forwards the decision making to bundle-router.
     */
    virtual  void forward(Message* msg); 
    
    /**
     * Execute the list of actions as returned by bundle-router
     */
    void execute_router_action(BundleAction* action);
    
    /**
     * Forward a BundleEvent to BundleRouter
     */
    void forward_event(BundleEvent* event) ;
    
    BundleRouter* router_;
    FloodConsumer * consumer_;
};

} // namespace dtnsim

#endif /* _GLUE_NODE_H_ */

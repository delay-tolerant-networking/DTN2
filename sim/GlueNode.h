/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _GLUE_NODE_H_
#define _GLUE_NODE_H_

/*
 * Node which interfaces to DTN2  and Simulator.
 * This node is responsible for forwarding/converting events
 * from Simulator to DTN2 and vice-versa.
 * It acts as  BundleForwarder. On receiving bundles/contact dynamics
 * etc... it informs the BundleRouter and  executes the returned
 * list of action. The actual forwarding of a Bundle using the
 * Simulator is done at the SimulatorConvergence layer.
 */


#include "Node.h"
#include "routing/BundleRouter.h"
#include "FloodConsumer.h"

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


#endif /* _GLUE_NODE_H_ */

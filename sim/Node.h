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
#ifndef _NODE_H_
#define _NODE_H_

#include <oasys/debug/Debug.h>
#include <oasys/debug/Log.h>

#include "EventHandler.h"
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
class Node : public EventHandler, public BundleDaemon {
public:
    /**
     * Constructor / Destructor
     */
    Node(const char* name);
    
    virtual ~Node() {}
        
    const char* name() { return name_.c_str(); }
    
    /**
     * Virtual function from processable
     */
    virtual void process(Event *e);

    /**
     * Accessor for router.
     */
    BundleRouter* router() { return router_; }

    /**
     * Set the node as the "active" node in the simulation. This
     * swings the static BundleDaemon::instance_ pointer to point to
     * the node so all singleton accesses throughout the code will
     * reference the correct node.
     */
    void set_active()
    {
        instance_ = this;
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
    BundleRouter*	router_;
    ContactManager*	contactmgr_;
};

} // namespace dtnsim

#endif /* _NODE_H_ */

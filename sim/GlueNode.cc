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
#include "GlueNode.h"

#include "bundling/Bundle.h"
#include "bundling/BundleAction.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleConsumer.h"
#include "bundling/Contact.h"

#include "SimConvergenceLayer.h"

namespace dtn {

GlueNode::GlueNode(int id,const char* logpath): Node(id,logpath) 
{
    router_ = BundleRouter::create_router(BundleRouter::type_.c_str());
    log_info("N[%d]: creating router of type:%s",id,BundleRouter::type_.c_str());
    
    consumer_ = NULL;
}


void 
GlueNode::message_received(Message* msg) 
{

    if (msg->dst() == id()) {
        log_info("RCV[%d]: src:%d id:%d, size-rcv %f",
                 id(),msg->src(),msg->id(),msg->size());
    }
    else {
        log_info("FWD[%d]: src:%d id:%d, size-rcv %f",
                 id(),msg->src(),msg->id(),msg->size());
    }
    forward(msg);
}

void forward_event(BundleEvent* event) ;


void 
GlueNode::chewing_complete(SimContact* c, double size, Message* msg) 
{

    bool acked = true;
    Bundle* bundle = SimConvergenceLayer::msg2bundle(msg);
    Contact* consumer = SimConvergenceLayer::simlink2ct(c);
    int tsize = (int)size;
    BundleTransmittedEvent* e = 
        new BundleTransmittedEvent(bundle,consumer,tsize,acked);
    forward_event(e);
    
}

void 
GlueNode::open_contact(SimContact* c) 
{
    Link* link = SimConvergenceLayer::simlink2dtnlink(c);
    LinkCreatedEvent* e = new LinkCreatedEvent(link);
    log_debug("N[%d]: C:%d [%d->%d]:UP",id(),c->id(),id(),c->dst()->id());
    forward_event(e);
}


void 
GlueNode::close_contact(SimContact* c)
{
    Contact* ct = SimConvergenceLayer::simlink2ct(c);
    
    // ct may be null when the contact was never created
    if (ct != NULL) {
        ContactDownEvent* e = new ContactDownEvent(ct);
        log_debug("N[%d]: C:%d [%d->%d]:DOWN",id(),c->id(),id(),c->dst()->id());
        forward_event(e);
    }
}


void
GlueNode::process(Event* e) {
    
    switch (e->type()) {
    case MESSAGE_RECEIVED:    {
        Event_message_received* e1 = (Event_message_received*)e;
        Message* msg = e1->msg_;
        log_info("GOT[%d]: id:%d size:%3f",id(),msg->id(),msg->size());
        
        // update the size of the message that is received
        msg->set_size(e1->sizesent_);
        message_received(msg);
        break;
    }
        
    case FOR_BUNDLE_ROUTER: {
        BundleEvent* be = ((Event_for_br* )e)->bundle_event_;
        forward_event(be);
        break;
    }
    default:
        PANIC("unimplemented action code");
    }
}


void
GlueNode::forward(Message* msg) 
{
    // first see if there exists a  bundle in the global hashtable
    Bundle *b = SimConvergenceLayer::msg2bundle(msg); 
    forward_event(new BundleReceivedEvent(b, EVENTSRC_PEER));
}



/**
 * Routine that actually effects the forwarding operations as
 * returned from the BundleRouter.
 */
void
GlueNode::execute_router_action(BundleAction* action)
{
    Bundle* bundle;
    bundle = action->bundleref_.bundle();
    
    switch (action->action_) {
    case ENQUEUE_BUNDLE: {
        BundleEnqueueAction* enqaction = (BundleEnqueueAction*)action;

        BundleConsumer* bc = enqaction->nexthop_;

        if (bc->is_local()) {
            log_info("N[%d] reached destination id:%d",
                     id(), bundle->bundleid_);
        } else {
            log_info("N[%d] enqueue id:%d as told by routercode",
                     id(), bundle->bundleid_);
        }
        
        bc->enqueue_bundle(bundle, &enqaction->mapping_);
        break;
    }
    case STORE_ADD: { 
        log_debug("N[%d] storing ignored %d", id(), bundle->bundleid_);
        break;
    }
    
    case STORE_DEL: {
        log_debug("N[%d] deletion ignored %d", id(), bundle->bundleid_);
        break;
    }
    default:
        PANIC("unimplemented action code %s",
              bundle_action_toa(action->action_));
    }
    delete action;
}


void 
GlueNode::forward_event(BundleEvent* event) 
{

    BundleActionList actions;
    BundleActionList::iterator iter;
    
    ASSERT(event);
    
    // always clear the action list
    actions.clear();
    
    // dispatch to the router to fill in the actions list
    router_->handle_event(event, &actions);
    
    // process the actions
    for (iter = actions.begin(); iter != actions.end(); ++iter) {
        execute_router_action(*iter);
    }
    
}



void    
GlueNode::create_consumer()
{
    consumer_ = new FloodConsumer(id_,"bundles://sim/simcl://2");
    consumer_->set_router(router_);

    RegistrationAddedEvent *reg_add = new RegistrationAddedEvent(consumer_);
    forward_event(reg_add);
}

} // namespace dtn

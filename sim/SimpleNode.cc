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
#include "SimpleNode.h"

SimpleNode::SimpleNode( int id, const char* logpath) 
    : Node(id,logpath),msgq_(0)
{
}

SimpleNode::~SimpleNode() {}


void 
SimpleNode::chewing_complete(SimContact* c, double size, Message* msg) 
{
    
    if (msg->size() != size){

	/**
	 * If the whole message is not sent create 
	 * a new message (using the clone function) and sets its offsets
	 * appropriately.
	 */
	
	log_info("ChXXX for msg %d, frac %f",msg->id(),size*1.0/msg->size());
	if (size != 0) {
	    msg = (Message* )msg->clone();
	    msg->rm_bytes(size);
	}
	// Forward the new fragment
	forward(msg);
    }
    else {
	log_info("ChC  for msg %d",msg->id());
    }
}

/**
 * Checks if the contact is a relevant next hop for 
 * a message. This particular function declars a contact
 * relevant as long as the contact destination is not the
 * same as message source
 */
bool 
is_relevant(SimContact* c, Message* m) 
{
    if (m->src() == c->dst()->id()) return false;
    return true;
}

/**
 * Find any available contact and forward the message on it.
 */
void
SimpleNode::forward(Message* msg) 
{
    for(int i=0;i<Topology::contacts();i++){
	SimContact *c = Topology::contact(i);
	if (c->is_open() && (c->src()->id() == id() )) {
	    
	    if (is_relevant(c,msg)) {
		c->chew_message(msg);
		return;
	    }
	}
    }
    /* No one ate the message, so store it */
    msgq_.push_back(msg);
}

/**
 * Send any pending message  over the new contact
 */
void 
SimpleNode::open_contact(SimContact* ct) 
{
    for(u_int i=0;i<msgq_.size();i++) {
	Message* msg =  msgq_[i];
	if (is_relevant(ct,msg)) {
	    
	    log_debug("contact is open and valid for messaged %d",msg->id());
	    std::vector<Message*>::iterator rmv = msgq_.begin() + i - 1;
	    
	    // now, actually remove this element from the array
	    msgq_.erase(rmv);
	    ct->chew_message(msg);
	    return;
	}
    }

}

void 
SimpleNode::close_contact(SimContact* c) 
{
// Do nothing
}


void
SimpleNode::message_received(Message* msg) 
{
    if (msg->dst() == id()) {
	// Local message, declare it to be received
	log_info("Rc msg at node %d, id %d, size-rcv %f",
		 id(),msg->id(),msg->size());
    }
    else {
	// Forward it to appropriate next hop
	log_info("Fw msg at node %d, id %d, size-rcv %f",
		 id(),msg->id(),msg->size());
	forward(msg);
    }
}


void
SimpleNode::process(Event* e) 
{
    
    if (e->type() == MESSAGE_RECEIVED) {
	
	Event_message_received* e1 = (Event_message_received*)e;
	Message* msg = e1->msg_;
	log_info("received msg (%d) size %3f",msg->id(),msg->size());
	
	// update the size of the message that is received
	msg->set_size(e1->sizesent_);
	message_received(msg);
	
    }
    else
	PANIC("unimplemented action code");
}

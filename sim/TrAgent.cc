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

#include "TrAgent.h"
#include "Message.h"
#include "Simulator.h"
#include "Node.h"

namespace dtnsim {

TrAgent::TrAgent(double t,int src, int dst, int size, int batchsize, int reps, int gap) 
: Logger ("/sim/tragent") {
    start_time_ = t;
    src_ = src;
    dst_= dst;
    reps_ = reps;
    batchsize_ = batchsize;
    gap_ = gap;
    size_ = size;
    repsdone_ = 0;
  
}

void 
TrAgent::start() {
    Event* e1 = new Event(start_time_,this,TR_NEXT_SENDTIME);
    Simulator::add_event(e1);
}

void
TrAgent::send(double time, int size) {

    Message* msg = new Message(src_,dst_,size);
    Node* src_tmp = Topology::node(src_);
    Event_message_received *esend = new Event_message_received(time,src_tmp,size,NULL,msg);
    Simulator::add_event(esend);
    log_info("N[%d]: GEN id:%d N[%d]->N[%d] size:%d",src_,msg->id(),src_,dst_,size);
}

void
TrAgent::process(Event* e) {
    
    if (e->type() == TR_NEXT_SENDTIME) {
	for(int i=0;i<batchsize_;i++) {
	    send(Simulator::time(),size_);
	}
            
	// Increment # of reps
	repsdone_ ++;
	if (repsdone_ < reps_) {
	    double tmp = Simulator::time() + gap_ ;
	    Event* e1 = new Event(tmp,this,TR_NEXT_SENDTIME);
	    Simulator::add_event(e1);
	} 
	else {
	    log_info("All bacthes finished");
	}
    }
    else {
	log_debug("undefined event");
    }
}



} // namespace dtnsim

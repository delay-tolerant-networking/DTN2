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
#include "Topology.h"
#include "GlueNode.h"
#include "SimpleNode.h"
#include "SimConvergenceLayer.h"

namespace dtn {

Node* Topology::nodes_[MAX_NODES_];
SimContact* Topology::contacts_[MAX_CONTACTS_];

int Topology::num_cts_ = 0;
int Topology::node_type_ = 1;


void
Topology::create_node(int id)
{
    
    Node* nd;
    if (Topology::node_type_ == 1) {
	nd = new SimpleNode(id,"/sim/snode");
    } else if (Topology::node_type_ == 2) {
	nd = new GlueNode(id,"/sim/gluenode"); //DTN2-Simulator glue
    } else {
	PANIC("unimplemented node type");
    }

   Topology::nodes_[id] = nd;
}

void
Topology::create_consumer(int id)
{
    Node *node = Topology::nodes_[id];
    if (!node) {
        PANIC("Node:%d not created yet",id);
    }
    node->create_consumer();
}


void
Topology::create_contact(int id, int src, int dst, int bw, 
			int delay, int isup,  int up, int down) 
{
    // create DTN2 contact also
    if (Topology::node_type_ == 2) {
	SimConvergenceLayer::create_ct(id);
    }
    
    SimContact* simct = new SimContact(id,Topology::nodes_[src],
				       Topology::nodes_[dst],bw,delay,isup, up,down);
    Topology::contacts_[id] = simct;
    num_cts_++;
}


} // namespace dtn

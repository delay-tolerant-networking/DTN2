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
#ifndef _TOPOLOGY_H_
#define _TOPOLOGY_H_


/**
 * The class that maintains the topology of the network
 */

#include <vector>
#include <oasys/debug/Debug.h>
#include <oasys/debug/Log.h>

class Node;
class SimContact;


class Topology  {

public:
    static void create_node(int id);
    static void create_contact(int id, int src, int dst, 
			       int bw, int delay, int isup, int up, int down);
    static void create_consumer(int id);
    static const int MAX_NODES_  = 100;
    static const int MAX_CONTACTS_ = 1000;
    
    /**
     * Global configuration of node type.
     * Different node instances are created based upon node type
     * Currently supportes node types:
     * node_type 1 uses SimpleNode
     * node_type 2  uses GlueNode
     */
    static int node_type_ ;
    static int num_cts_ ;  ///> total number of contacts
    static int contacts() { return num_cts_ ; }

    static Node*  nodes_[];
    static SimContact* contacts_[];
   
    static Node* node(int i) { return nodes_[i] ; }
    static SimContact* contact(int i) { return contacts_[i] ; }

};

#endif /* _TOPOLOGY_H_ */

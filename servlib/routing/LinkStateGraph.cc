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

#include "BundleRouter.h"
#include "RouteTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleActions.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/Contact.h"
#include "bundling/FragmentManager.h"
#include "reg/Registration.h"
#include <stdlib.h>

#include "LinkStateGraph.h"


namespace dtn {
using namespace std;

LinkStateGraph::LinkStateGraph() {
    
}

LinkStateGraph::Vertex* 
LinkStateGraph::findNextHop(Vertex* from, Vertex *to) 
{
    priority_queue< Vertex*, vector<Vertex*>, less<Vertex*> > queue;
    Vertex* current;

    queue.push(from);

    /* first reset all the costs in the link state graph */
    for(set<Vertex*>::iterator i=vertices.begin(); i!=vertices.end(); i++)
        (*i)->dijkstra_distance_=100000;  //maxint?
    
    from->dijkstra_distance_=0;

    /* now compute the dijkstra distances for each node */
    while((current = queue.top()))
    {
        queue.pop();
        for(map<Vertex*, Edge*>::iterator e=from->outgoing_edges_.begin(); e!=from->outgoing_edges_.end(); e++)
        {
            Vertex* peer=(*e).first;
            Edge* edge=(*e).second;

            if(peer->dijkstra_distance_ >= current->dijkstra_distance_ + edge->cost_)
            {
                peer->dijkstra_distance_ = current->dijkstra_distance_ + edge->cost_;
                queue.push(peer);
            }            
        }
    }

    /* to get the next hop (or the entire path), walk backwards from the destination */
    current = to;
    while(true)
    {
        Vertex* best=(*current->incoming_edges_.begin()).first;
        for(map<Vertex*, Edge*>::iterator e=current->incoming_edges_.begin(); e != current->incoming_edges_.end(); e++)
        {
            if(best->dijkstra_distance_ >= current->dijkstra_distance_)
                best=(*e).first;
        }
       
        if(best == from) 
            return current;
        else
            current = best;
    }

}

void 
LinkStateGraph::addEdge(Vertex *from, Vertex *to, cost_t cost) {
    if(!from->outgoing_edges_[to]) {
        Edge* e=new Edge(from, to, cost);

        from->outgoing_edges_[to]=e;
        to->incoming_edges_[from]=e;
    }        
}

void 
LinkStateGraph::removeEdge(Vertex *from, Vertex *to) {
    if(from->outgoing_edges_[to]) {
        Edge* e=from->outgoing_edges_[to];

        from->outgoing_edges_[to]=0;
        to->incoming_edges_[from]=0;
        delete e;
    }        
}

LinkStateGraph::Vertex *
LinkStateGraph::getVertex(const char* eid) {
    Vertex * v;
    for(set<Vertex*>::iterator i=vertices.begin();i!=vertices.end();i++)
    {
        if(strcmp(eid,(*i)->eid_)==0) {
            log_debug("Found matching vertex for %s.",eid);
            return *i;
        }
    }
    
    v=new Vertex(eid);
    vertices.insert(v);
    return v;
}


LinkStateGraph::Vertex::Vertex(const char * eid) {
    memset(eid_,0,MAX_EID);
    strcpy(eid_,eid);    
    dijkstra_distance_=0;
}

LinkStateGraph::Edge::Edge(Vertex* from, Vertex* to, cost_t cost)
{
    from_=from;
    to_=to;
    cost_=cost;
}

} // namespace dtn

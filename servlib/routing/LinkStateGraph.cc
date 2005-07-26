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
#include <set>

#include "LinkStateGraph.h"


namespace dtn {
using namespace std;

LinkStateGraph::LinkStateGraph() {
    
}

/**
 *   Run the dijkstra algorithm, and then return the best next hop. This is very naive, but at least less bug prone.
 */

LinkStateGraph::Vertex* 
LinkStateGraph::findNextHop(Vertex* from, Vertex *to) 
{
    priority_queue< Vertex*, vector<Vertex*>, less<Vertex*> > queue;
    Vertex* current;
    
    if(!from || !to) 
    {
        log_info("LinkStateGraph","Can't route to null Vertex.");
        return 0;
    }

    queue.push(from);

    /* first reset all the costs in the link state graph */
    for(set<Vertex*>::iterator i=vertices_.begin(); i!=vertices_.end(); i++) 
        (*i)->dijkstra_distance_=100000;  //maxint?
    
    from->dijkstra_distance_=0;

    /* now compute the dijkstra distances for each node */
    while(!queue.empty())
    {
        current = queue.top();        
        queue.pop();
        for(map<Vertex*, Edge*>::iterator e=from->outgoing_edges_.begin(); e!=from->outgoing_edges_.end(); e++)
        {
            Vertex* peer=(*e).first;
            Edge* edge=(*e).second;

            ASSERT(peer && edge); // sanity check

            if(peer->dijkstra_distance_ >= current->dijkstra_distance_ + edge->cost_)
            {
                peer->dijkstra_distance_ = current->dijkstra_distance_ + edge->cost_;
                queue.push(peer);
            }            
        }
    }

    if(to->dijkstra_distance_ == 100000) {
        log_debug("LinkStateGraph","No link-state route to %s from %s.",to->eid_, from->eid_);
        return 0;
    }

    /* to get the next hop (or the entire path), walk backwards from the destination */
    current = to;
    while(true)
    {
        Vertex* best=(*current->incoming_edges_.begin()).first;
        for(map<Vertex*, Edge*>::iterator e=current->incoming_edges_.begin(); e != current->incoming_edges_.end(); e++)
        {
            if((*e).first->dijkstra_distance_ < best->dijkstra_distance_)
                best=(*e).first;
        }

        if(best == from) 
            return current;
        else            
            current = best;
    }
    return current;
}

LinkStateGraph::Edge*
LinkStateGraph::addEdge(Vertex *from, Vertex *to, cost_t cost) {
    if(!from->outgoing_edges_[to]) {
        Edge* e=new Edge(from, to, cost);

        from->outgoing_edges_[to]=e;
        to->incoming_edges_[from]=e;
        return e;
    }        
    else return 0;
}

LinkStateGraph::Edge*
LinkStateGraph::getEdge(Vertex *from, Vertex *to)
{
    return from->outgoing_edges_[to];
}

void 
LinkStateGraph::removeEdge(Edge* e) {
    Vertex *from=e->from_, *to=e->to_;
    
    ASSERT(from->outgoing_edges_[to]==e);
    ASSERT(to->incoming_edges_[from]==e);

    from->outgoing_edges_[to]=0;
    to->incoming_edges_[from]=0;

    delete e;
}

/**
 * Finds a vertex that matches the given eid. Only suffix matching is supported, as in "pattern*"
 **/
LinkStateGraph::Vertex *
LinkStateGraph::getMatchingVertex(const char* eid) {

    log_info("LinkStateGraph","getMatchingVertex has %u vertices.",(u_int)vertices_.size());

    for(set<Vertex*>::iterator iter=vertices_.begin();iter!=vertices_.end();iter++)
    {
        char* pattern=(*iter)->eid_;        
        unsigned int len=min(strlen(pattern),strlen(eid));

        log_info("LinkStateGraph","Matching pattern %s against eid %s.",pattern,eid);

        // compare the string against the pattern. 
        // XXX/jakob - this matching is pretty lame. what can we do 
        //             without making too many assumptions about eids? 
        for(unsigned int i=0;i<len;i++)
            if(pattern[i]!=eid[i] &&   
               !(i==strlen(pattern) && 
                 pattern[i]=='*')) 
                goto not_a_match;
                    
        // if we found a match, we're done  (XXX/jakob - not bothering with many matches at the moment)
        log_debug("LinkStateGraph","Found a match! %s matches %s",pattern,eid);
        return *iter;

 not_a_match:
        continue;
        // try the next vertex
    }    
    return 0;
}

LinkStateGraph::Vertex *
LinkStateGraph::getVertex(const char* eid) {
    Vertex * v;
    for(set<Vertex*>::iterator i=vertices_.begin();i!=vertices_.end();i++)
    {
        if(strcmp(eid,(*i)->eid_)==0) {
            log_debug("LinkStateGraph","Found matching vertex for %s.",eid);
            return *i;
        }
    }
    
    v=new Vertex(eid);
    vertices_.insert(v);
    return v;
}

void
LinkStateGraph::dumpGraph(oasys::StringBuffer* buf)
{
    // XXX/jakob - later on, this also needs to dump the link schedule, if one exists.
    for(set<Edge*>::iterator i=edges_.begin();i!=edges_.end();i++)
        buf->appendf("%s %s %d",(*i)->from_->eid_, (*i)->to_->eid_, (*i)->cost_); 
}

std::set<LinkStateGraph::Edge*> 
LinkStateGraph::edges()
{
    return edges_;
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
    contact_.start=0;
    contact_.duration=0;
}

} // namespace dtn

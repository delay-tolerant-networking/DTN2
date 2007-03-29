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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <set>
#include <queue>

#include "LinkStateGraph.h"


namespace dtn {
using namespace std;

//////////////////////////////////////////////////////////////////////////////
LinkStateGraph::LinkStateGraph() 
    : Logger("LinkStateGraph", "/dtn/route/linkstate/graph") 
{}

//////////////////////////////////////////////////////////////////////////////
LinkStateGraph::Vertex* 
LinkStateGraph::findNextHop(Vertex* from, Vertex *to) 
{
    priority_queue< Vertex*, vector<Vertex*>, less<Vertex*> > queue;
    Vertex* current;
    
    if(!from || !to) 
    {
        log_info("Can't route to null Vertex.");
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
        for(map<Vertex*, Edge*>::iterator e=from->outgoing_edges_.begin(); 
            e!=from->outgoing_edges_.end(); e++)
        {
            Vertex* peer=(*e).first;
            Edge* edge=(*e).second;

            ASSERT(peer && edge); // sanity check

            if(peer->dijkstra_distance_ >= current->dijkstra_distance_ + 
               edge->cost_)
            {
                peer->dijkstra_distance_ = current->dijkstra_distance_ + 
                                           edge->cost_;
                queue.push(peer);
            }            
        }
    }

    if(to->dijkstra_distance_ == 100000) {
        log_debug( "No link-state route to %s from %s.",to->eid_, from->eid_);
        return 0;
    }

    /* to get the next hop (or the entire path), walk backwards from
     * the destination */
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

//////////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////////
LinkStateGraph::Edge*
LinkStateGraph::getEdge(Vertex *from, Vertex *to)
{
    return from->outgoing_edges_[to];
}

//////////////////////////////////////////////////////////////////////////////
void 
LinkStateGraph::removeEdge(Edge* e) 
{
    Vertex *from=e->from_, *to=e->to_;
    
    ASSERT(from->outgoing_edges_[to]==e);
    ASSERT(to->incoming_edges_[from]==e);

    from->outgoing_edges_[to]=0;
    to->incoming_edges_[from]=0;

    delete e;
}

//////////////////////////////////////////////////////////////////////////////
LinkStateGraph::Vertex *
LinkStateGraph::getMatchingVertex(const char* eid) {

    log_info("getMatchingVertex has %zu vertices.",
             vertices_.size());
    
    for(set<Vertex*>::iterator iter=vertices_.begin();iter!=vertices_.end();iter++)
    {
        char* pattern=(*iter)->eid_;        
        unsigned int len=min(strlen(pattern),strlen(eid));

        log_info("Matching pattern %s against eid %s.",pattern,eid);

        // compare the string against the pattern. 
        // XXX/jakob - this matching is pretty lame. what can we do 
        //             without making too many assumptions about eids? 
        for(unsigned int i=0;i<len;i++)
            if(pattern[i]!=eid[i] &&   
               !(i==strlen(pattern) && 
                 pattern[i]=='*')) 
                goto not_a_match;
                    
        // if we found a match, we're done  (XXX/jakob - not bothering with many matches at the moment)
        log_debug("Found a match! %s matches %s",pattern,eid);
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
            log_debug("Found matching vertex for %s.",eid);
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

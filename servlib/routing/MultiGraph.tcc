/*
 *    Copyright 2007 Intel Corporation
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

#include "MultiGraph.h"
#include <oasys/util/StringAppender.h>
#include <oasys/util/UpdatablePriorityQueue.h>

namespace dtn {

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline MultiGraph<_NodeInfo,_EdgeInfo>
::MultiGraph()
    : Logger("MultiGraph", "/dtn/route/graph")
{
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline MultiGraph<_NodeInfo,_EdgeInfo>
::~MultiGraph()
{
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline class MultiGraph<_NodeInfo,_EdgeInfo>::Node*
MultiGraph<_NodeInfo,_EdgeInfo>
::add_node(const std::string& id, const _NodeInfo& info)
{
    Node* n = new Node(id, info);
    this->nodes_.push_back(n);
    return n;
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline class MultiGraph<_NodeInfo,_EdgeInfo>::Node*
MultiGraph<_NodeInfo,_EdgeInfo>
::find_node(const std::string& id) const
{
    class NodeVector::const_iterator iter;
    for (iter = this->nodes_.begin(); iter != this->nodes_.end(); ++iter) {
        if ((*iter)->id_ == id) {
            return *iter;
        }
    }
    return NULL;
}
    
//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline bool
MultiGraph<_NodeInfo,_EdgeInfo>
::del_node(const std::string& id)
{
    class NodeVector::iterator iter;
    for (iter = this->nodes_.begin(); iter != this->nodes_.end(); ++iter) {
        if ((*iter)->id_ == id) {
            delete (*iter);
            this->nodes_.erase(iter);
            return true;
        }
    }

    return false;
}
    
//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline void
MultiGraph<_NodeInfo,_EdgeInfo>
::clear()
{
    class NodeVector::iterator iter;
    for (iter = this->nodes_.begin(); iter != this->nodes_.end(); ++iter) {
        delete (*iter);
    }

    this->nodes_.clear();
}
    
//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline class MultiGraph<_NodeInfo,_EdgeInfo>::Edge*
MultiGraph<_NodeInfo,_EdgeInfo>
::add_edge(Node* a, Node* b, const _EdgeInfo& info)
{
    Edge* e = new Edge(a, b, info);
    a->out_edges_.push_back(e);
    b->in_edges_.push_back(e);
    return e;
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline class MultiGraph<_NodeInfo,_EdgeInfo>::Edge*
MultiGraph<_NodeInfo,_EdgeInfo>
::find_edge(Node* a, Node* b, const _EdgeInfo& info)
{
    class EdgeVector::const_iterator iter;
    for (iter = a->out_edges_.begin(); iter != a->out_edges_.end(); ++iter) {
        if (*iter->info_ == info) {
            return *iter;
        }
    }

    return NULL;
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline bool
MultiGraph<_NodeInfo,_EdgeInfo>
::del_edge(Node* node, Edge* edge)
{
    class EdgeVector::iterator ei, ei2;
    for (ei = node->out_edges_.begin();
         ei != node->out_edges_.end(); ++ei)
    {
        if (*ei == edge)
        {
            // delete the corresponding in_edge pointer, which must
            // exist or else there's some internal inconsistency
            ei2 = edge->dest_->in_edges_.begin();
            while (1) {
                ASSERTF(ei2 != edge->dest_->in_edges_.end(),
                        "out_edge / in_edge mismatch!!");
                if (*ei2 == edge)
                    break;
                ++ei2;
            }
            
            node->out_edges_.erase(ei);
            edge->dest_->in_edges_.erase(ei2);
            delete edge;
            return true;
        }
    }
    
    return false;
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline void
MultiGraph<_NodeInfo,_EdgeInfo>
::shortest_path(Node* a, Node* b, EdgeVector* path, WeightFn* weight_fn)
{
    ASSERT(a != NULL);
    ASSERT(b != NULL);
    ASSERT(path != NULL);
    ASSERT(a != b);
    path->clear();

    // first clear the existing distances
    for (class NodeVector::iterator i = this->nodes_.begin();
         i != this->nodes_.end(); ++i)
    {
        (*i)->distance_ = 0xffffffff;
        (*i)->color_    = Node::WHITE;
        (*i)->prev_     = NULL;
    }
    
    // compute dijkstra distances
    oasys::UpdatablePriorityQueue<Node*, std::vector<Node*>, DijkstraCompare> q;
    
    a->distance_ = 0;
    q.push(a);
    do {
        Node* cur = q.top();
        q.pop();

        for (class EdgeVector::iterator ei = cur->out_edges_.begin();
             ei != cur->out_edges_.end(); ++ei)
        {
            Edge* edge = *ei;
            Node* peer = edge->dest_;

            ASSERT(edge->source_ == cur);
            ASSERT(peer != cur); // no loops
            
            u_int32_t weight = (*weight_fn)(edge);

            log_debug("examining edge id %s (weight %u) "
                      "from %s (distance %u) -> %s (distance %u)",
                      oasys::InlineFormatter<_EdgeInfo>().format(edge->info_),
                      weight, cur->id_.c_str(), cur->distance_,
                      peer->id_.c_str(), peer->distance_);
            
            if (peer->distance_ > cur->distance_ + weight)
            {
                peer->distance_ = cur->distance_ + weight;
                peer->prev_     = edge;

                if (peer->color_ == Node::WHITE)
                {
                    log_debug("pushing node %s (distance %u) onto queue",
                              peer->id_.c_str(), peer->distance_);
                    peer->color_ = Node::GRAY;
                    q.push(peer);
                }
                else if (peer->color_ == Node::GRAY)
                {
                    log_debug("updating node %s in queue", peer->id_.c_str());
                    q.update(peer);
                }
                else
                {
                    PANIC("can't revisit a black node!");
                }
            }
        }

        cur->color_ = Node::BLACK;

        // safe to bail once we reach the destination
        if (cur == b) {
            break;
        }
             
    } while (!q.empty());
    
    if (b->distance_ == 0xffffffff) {
        return; // no path
    }

    // now traverse the graph backwards from the destination, filling
    // in the path
    Node* cur = b;
    do {
        ASSERT(cur->prev_->dest_ == cur);
        ASSERT(cur->prev_->source_ != cur);

        path->push_back(cur->prev_);
        cur = cur->prev_->source_;
    } while (cur != a);
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline class MultiGraph<_NodeInfo,_EdgeInfo>::Edge*
MultiGraph<_NodeInfo,_EdgeInfo>
::best_next_hop(Node* a, Node* b, WeightFn* weight_fn)
{
    EdgeVector path;
    shortest_path(a, b, &path, weight_fn);
    if (path.empty()) {
        return NULL;
    }

    ASSERT(path[0].source_ == a); // sanity
    return path[0];
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline int
MultiGraph<_NodeInfo,_EdgeInfo>
::format(char* buf, size_t sz) const
{
    oasys::StringAppender sa(buf, sz);
    
    class NodeVector::const_iterator ni;
    class EdgeVector::const_iterator ei;
    
    for (ni = this->nodes_.begin(); ni != this->nodes_.end(); ++ni)
    {
        sa.appendf("%s ->", (*ni)->id_.c_str());
        
        for (ei = (*ni)->out_edges_.begin();
             ei != (*ni)->out_edges_.end(); ++ei)
        {
            sa.appendf(" %s(%s)", 
                       (*ei)->dest_->id_.c_str(),
                       oasys::InlineFormatter<_EdgeInfo>().format((*ei)->info_));
        }
        sa.append("\n");
    }

    return sa.desired_length();
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline int
MultiGraph<_NodeInfo,_EdgeInfo>
::NodeVector::format(char* buf, size_t sz) const
{
    class oasys::StringAppender sa(buf, sz);
    class NodeVector::const_iterator i;
    for (i = this->begin(); i != this->end(); ++i)
    {
        sa.appendf("%s%s",
                   i == this->begin() ? "" : " ",
                   (*i)->id_.c_str());
    }
    return sa.desired_length();
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline int
MultiGraph<_NodeInfo,_EdgeInfo>
::EdgeVector::format(char* buf, size_t sz) const
{
    class oasys::StringAppender sa(buf, sz);
    class EdgeVector::const_iterator i;
    for (i = this->begin(); i != this->end(); ++i)
    {
        sa.appendf("%s[%s -> %s(%s)]",
                   i == this->begin() ? "" : " ",
                   (*i)->source_->id_.c_str(),
                   (*i)->dest_->id_.c_str(),
                   oasys::InlineFormatter<_EdgeInfo>().format((*i)->info_));
    }
    return sa.desired_length();
}


} // namespace dtn


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

#ifndef _MULTIGRAPH_H_
#define _MULTIGRAPH_H_

#include <vector>
#include <oasys/debug/InlineFormatter.h>
#include <oasys/debug/Formatter.h>
#include <oasys/debug/Logger.h>
#include <oasys/util/StringBuffer.h>

namespace dtn {

/**
 * Data structure to represent a multigraph.
 */
template <typename _NodeInfo, typename _EdgeInfo>
class MultiGraph : public oasys::Formatter, public oasys::Logger {
public:
    /// @{ Forward Declarations of helper classes
    class Edge;
    class EdgeVector;
    class Node;
    class NodeVector;
    class WeightFn;
    /// @}
    
    /// Constructor
    MultiGraph();

    /// Destructor
    ~MultiGraph();

    /// Add a new node
    Node* add_node(const std::string& id, const _NodeInfo& info);

    /// Find a node with the given id
    Node* find_node(const std::string& id) const;
    
    /// Delete a node and all its edges
    bool del_node(const std::string& id);
    
    /// Add an edge
    Edge* add_edge(Node* a, Node* b, const _EdgeInfo& info);

    /// Find an edge
    Edge* find_edge(Node* a, Node* b, const _EdgeInfo& info);
    
    /// Remove the specified edge from the given node, deleting the
    /// Edge object
    bool del_edge(Node* node, Edge* edge);
    
    /// Find the shortest path between two nodes by running Dijkstra's
    /// algorithm, filling in the edge vector with the best path
    void shortest_path(Node* a, Node* b, EdgeVector* path, WeightFn* weight_fn);

    /// More limited version of the shortest path that just returns
    /// the next hop edge.
    Edge* best_next_hop(Node* a, Node* b, WeightFn* weight_fn);

    /// Clear the contents of the graph
    void clear();

    /// Virtual from Formatter
    int format(char* buf, size_t sz) const;

    /// Return a string dump of the graph
    std::string dump() const
    {
        char buf[1024];
        int len = format(buf, sizeof(buf));
        return std::string(buf, len);
    }

    /// Accessor for the nodes array
    const NodeVector& nodes() { return nodes_; }

    /// The abstract weight function class
    class WeightFn {
    public:
        virtual ~WeightFn() {}
        virtual u_int32_t operator()(Edge* edge) = 0;
    };

    /// The node class
    class Node {
    public:
        /// Constructor
        Node(const std::string& id, const _NodeInfo info)
            : id_(id), info_(info) {}

        std::string id_;
        _NodeInfo   info_;
        
        EdgeVector  out_edges_;
        EdgeVector  in_edges_;

    private:
        friend class MultiGraph;
        friend class MultiGraph::DijkstraCompare;
        
        /// @{ Dijkstra algorithm state
        u_int32_t distance_;
        enum {
            WHITE, GRAY, BLACK
        } color_;
        Edge* prev_;
        /// @} 
    };

    /// Helper class to compute Dijkstra distance
    struct DijkstraCompare {
        bool operator()(Node* a, Node* b) const {
            return a->distance_ > b->distance_;
        }
    };

    /// The edge class.
    class Edge {
    public:
        /// Constructor
        Edge(Node* s, Node* d, const _EdgeInfo info)
            : source_(s), dest_(d), info_(info) {}
        
        Node*     source_;
        Node*     dest_;
        _EdgeInfo info_;
    };

    /// Helper data structure for a vector of nodes
    class NodeVector : public oasys::Formatter,
                       public std::vector<Node*> {
    public:
        int format(char* buf, size_t sz) const;
        std::string dump() const
        {
            char buf[1024];
            int len = format(buf, sizeof(buf));
            return std::string(buf, len);
        }
    };
    
    /// Helper data structure for a vector of edges
    class EdgeVector : public oasys::Formatter,
                       public std::vector<Edge*> {
    public:
        int format(char* buf, size_t sz) const;
        std::string dump() const
        {
            char buf[1024];
            int len = format(buf, sizeof(buf));
            return std::string(buf, len);
        }
    };

protected:
    /// The vector of all nodes
    NodeVector nodes_;
};

} // namespace dtn

#include "MultiGraph.tcc"

#endif /* _MULTIGRAPH_H_ */

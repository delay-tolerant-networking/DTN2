/*
 *    Copyright 2007 Baylor University
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

#ifndef _PROPHET_TABLE_H_
#define _PROPHET_TABLE_H_

#include "Util.h"
#include "Node.h"
#include "Bundle.h"
#include "BundleCore.h"

#include <map>
#include <list>

namespace prophet
{

class Dictionary; 

/**
 * Compare object for Heap
 */
struct heap_compare {
    bool operator() (Node* a, Node* b) {
        if (a == NULL || b == NULL) return false;
        return ((*a).p_value() > (*b).p_value());
    }
};

/**
 * Swap object for NodeHeap
 */
struct heap_pos {
    inline void operator()(Node* a, size_t pos)
    {
        a->set_heap_pos(pos);
    }
};

/**
 * Container for Prophet nodes (routes and predictability values).
 * Assumes ownership of memory pointed to by member Node*'s
 */
class Table
{
public:
    typedef std::map<std::string,Node*,less_string> rib_table;
    typedef std::map<std::string,Node*,less_string>::const_iterator
        const_iterator;
    typedef std::vector<Node*> Sequence;
    typedef Sequence::const_iterator heap_iterator;

    /**
     * Default constructor
     */
    Table(BundleCore* core,
          const std::string& name,
          bool persistent = false);

    /**
     * Copy constructor
     */
    Table(const Table& t);

    /**
     * Destructor
     */
    ~Table();

    /**
     * Find and return route for given destination ID; else return NULL
     */
    const Node* find(const std::string& dest_id) const;

    /**
     * Table takes ownership of memory pointed to by Node*, updating 
     * its member list (replacing element if n->dest_id() already exists)
     */
    void update(Node* n);

    /**
     * Convenience function for updating directly-encountered route in
     * table. Adds new node to table, or updates if route already exists.
     * On success, post-condition is that update_pvalue is called on the
     * node, then the node is updated in Table.
     */
    void update_route(const std::string& dest_id,
                      bool relay = Node::DEFAULT_RELAY,
                      bool custody = Node::DEFAULT_CUSTODY,
                      bool internet = Node::DEFAULT_INTERNET);

    /**
     * Convenience function for updating transitively-discovered route
     * in table. Creates new node, else updates existing. On success,
     * post-condition is update_transitive is called on node, then node
     * is updated in Table.
     */
    void update_transitive(const std::string& dest_id,
                           const std::string& peer_id,
                           double peer_pvalue,
                           bool relay = Node::DEFAULT_RELAY,
                           bool custody = Node::DEFAULT_CUSTODY,
                           bool internet = Node::DEFAULT_INTERNET);

    /**
     * Convenience wrapper around update_transitive to import entire RIB
     */
    void update_transitive(const std::string& peer_id,
                           const RIBNodeList& nodes,
                           const Dictionary& ribd);

    /**
     * Convenience function for looking up predictability of a given route
     */
    double p_value(const std::string& dest_id) const;

    /**
     * Convenience function for lookup of predictability of route to b's dest
     */
    double p_value(const Bundle* b) const;

    /**
     * Create duplicate list of Nodes, return number of elements
     */
    size_t clone(NodeList& list) const;

    /**
     * Returns number of routes held by Table
     */
    size_t size() const { return table_.size(); }

    /**
     * Set upper limit on number of routes retained by Table
     */
    void set_max_route(u_int max_route)
    {
        max_route_ = max_route;
        enforce_quota();
    }

    /**
     * The predictability of Nodes should diminish with time; to allow for
     * maintenance of routes, the Prophet spec allows for removing routes
     * whose predictability falls below some arbitrary epsilon.
     *
     * Given epsilon as a minimum predictability, remove all routes whose
     * predictability is below epsilon.  Return the number of routes
     * removed.
     */
    size_t truncate(double epsilon);

    /**
     * Update Table from peer's RIB
     */
    void assign(const RIBNodeList& list,
                const Dictionary& ribd);

    /**
     * Persistent storage interface: clear contents of Table and assign
     * from deserialization routine
     */
    void assign(const std::list<const Node*>& list,
                const NodeParams* params);

    /**
     * For maintenance routines, visit each Node in table and invoke its
     * aging algorithm; return the number of Nodes visited.
     */
    size_t age_nodes();

    ///@{ Iterators
    const_iterator begin() const { return table_.begin(); }
    const_iterator end() const { return table_.end(); }
    heap_iterator heap_begin() const { return heap_.sequence().begin(); }
    heap_iterator heap_end() const { return heap_.sequence().end(); }
    ///@}

protected:
    typedef rib_table::iterator iterator;
    typedef Heap<Node*,
                 std::vector<Node*>,
                 struct heap_compare,
                 struct heap_pos> NodeHeap;

    /**
     * Add route to heap
     */
    void heap_add(Node* n);

    /**
     * Remove node from heap
     */
    void heap_del(Node* n);

    /**
     * Enforce upper bound by eliminating minimum routes
     */
    void enforce_quota();

    /**
     * Clean up memory and remove pointer from internal map for this 
     * element
     */
    void remove(iterator* i);

    /**
     * Clean up memory pointed to by each Node*
     */
    void free();

    /**
     * Utility function for finding route by dest_id
     */
    bool find(const std::string& dest_id, iterator* i);

    BundleCore* const core_; ///< facade interface into Bundle host
    rib_table table_; ///< Mapped collection of <dest_id,Node*> pairs
    NodeHeap heap_; ///< Min-heap for quota-enforcement eviction ordering
    bool persistent_; ///< whether to utilize BundleCore's persistent
                      ///  storage interface for Nodes
    std::string name_; ///< object name
    u_int  max_route_; ///< upper limit to number of routes to retain
}; //class Table

}; //namespace prophet

#endif // _PROPHET_TABLE_H_

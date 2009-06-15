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

#ifndef _PROPHET_NODE_H_
#define _PROPHET_NODE_H_

#include <sys/types.h>
#include <string>
#include "PointerList.h"

namespace prophet
{

// forward declaration
class Table;
class RIBTLV;
class NodeHeap;

/**
 * Tunable parameter struct for setting global default values
 * for Prophet node algorithms
 */
struct NodeParams
{
    /**
     * Default initialization values, p. 15, 3.3, figure 2
     */
    static const double DEFAULT_P_ENCOUNTER;

    /**
     * Default initialization values, p. 15, 3.3, figure 2
     */
    static const double DEFAULT_BETA;

    /**
     * Default initialization values, p. 15, 3.3, figure 2
     */
    static const double DEFAULT_GAMMA;

    /**
     * The kappa variable describes how many 
     * milliseconds-per-timeunit (for equation 2, p.9, section 2.1.1)
     */
    static const u_int DEFAULT_KAPPA;

    NodeParams()
        : encounter_(DEFAULT_P_ENCOUNTER),
          beta_(DEFAULT_BETA),
          gamma_(DEFAULT_GAMMA),
          kappa_(DEFAULT_KAPPA) {}

    virtual ~NodeParams() {}

    double encounter_; ///< initial value for p_value
    double beta_;      ///< weighting factor for transitive algorithm
    double gamma_;     ///< weighting factor for aging algorithm
    u_int  kappa_;     ///< milliseconds per unit time, aging algorithm
}; // NodeParams

/**
 * Node represents a route to another Prophet node, and as such,
 * tracks destination endpoint ID and delivery predictability (0 <= p <= 1).
 * Pages and paragraphs refer to the Prophet Internet Draft released
 * March 2006
 */
class Node
{
public:
    static const bool DEFAULT_RELAY = true;
    static const bool DEFAULT_CUSTODY = true;
    static const bool DEFAULT_INTERNET = false;
    /**
     * Default constructor
     */
    Node(const NodeParams* params = NULL);

    /**
     * Copy constructor
     */
    Node(const Node& node);

    /**
     * Constructor
     */
    Node(const std::string& dest_id,
         bool relay = DEFAULT_RELAY,
         bool custody = DEFAULT_CUSTODY,
         bool internet = DEFAULT_INTERNET,
         const NodeParams* params = NULL);

    /**
     * Destructor
     */
    virtual ~Node();

    ///@{ Accessors
    double      p_value()      const { return p_value_; }
    bool        relay()        const { return relay_; }
    bool        custody()      const { return custody_; }
    bool        internet_gw()  const { return internet_gateway_; }
    virtual const char* dest_id() const { return dest_id_.c_str(); }
    const std::string &dest_id_ref() const { return dest_id_; }
    u_int32_t   age()          const { return age_; }
    const NodeParams* params() const { return params_; }
    ///@}

    /**
     * Assignment operator
     */
    Node& operator= (const Node& n)
    {
        p_value_ = n.p_value_;
        relay_   = n.relay_;
        custody_ = n.custody_;
        internet_gateway_ = n.internet_gateway_;
        dest_id_.assign(n.dest_id_);
        age_     = n.age_;
        delete params_;
        params_  = new NodeParams(*n.params_);
        heap_pos_ = n.heap_pos_;
        return *this;
    }

    /**
     * Apply the direct contact algorithm, p. 9, 2.1.1, eq. 1
     */
    void update_pvalue();

    /**
     * Apply transitive algorithm, where ab is p_value from local 
     * to peer, bc is p_value from peer to this node, p. 10, 2.1.1, eq. 3
     */
    void update_transitive(double ab, double bc);

    /**
     * Routes must decrease predictability with the passing of time,
     * p. 9, 2.1.1, eq. 2
     */
    void update_age();

    ///@{ Accessor and mutator for heap index used by Table and testing
    size_t heap_pos() { return heap_pos_; }
    void set_heap_pos(size_t pos) { heap_pos_ = pos; }
    ///}

protected:
    friend class Table;
    friend class RIBTLV;

    ///@{ Mutators, protected for use by friend classes only
    void set_pvalue( double d )
    {
        if ( d >= 0.0 && d <= 1.0 ) p_value_ = d;
    };
    void set_relay( bool relay ) { relay_ = relay; }
    void set_custody( bool custody ) { custody_ = custody; }
    void set_internet_gw( bool gw ) { internet_gateway_ = gw; }
    virtual void set_dest_id( const std::string& eid )
    {
        dest_id_.assign(eid);
    }
    void set_age( u_int32_t age ) { age_ = age; }
    void set_params( const NodeParams* params )
    {
        delete params_;
        if (params != NULL)
            params_ = new NodeParams(*params);
        else
            // take the defaults
            params_ = new NodeParams();
    }
    ///@}

    /**
     * Use NodeParams::kappa_ milliseconds per unit to convert diff
     * to time units for use in Equation 2
     */
    u_int time_to_units(u_int32_t diff) const;

    const NodeParams* params_; ///< global settings for all prophet nodes
    double            p_value_; ///< predictability value for this node
    bool              relay_; ///< whether this node acts as relay
    bool              custody_; ///< whether this node accepts custody txfr
    bool              internet_gateway_; ///< whether bridge to Internet
    std::string       dest_id_; ///< string representation of route to node
    u_int32_t         age_; ///< age in seconds of last update to p_value
    size_t            heap_pos_; ///< heap index used by Table
}; // Node

/**
 * RIBNode provides a convenience wrapper around Node for tracking
 * endpoint ID to string ID conversions while serializing/deserializing
 */
class RIBNode : public Node
{
public:
    /**
     * Constructor
     */
    RIBNode(const Node* node, u_int16_t sid)
        : Node(*node), sid_(sid) {}

    /**
     * Default constructor
     */
    RIBNode(u_int16_t sid = 0)
        : Node(), sid_(sid) {}

    /**
     * Copy constructor
     */
    RIBNode(const RIBNode& a)
        : Node(a), sid_(a.sid_) {}

    /**
     * Destructor
     */
    virtual ~RIBNode() {}

    /**
     * Assignment operator
     */
    RIBNode& operator= (const RIBNode& a) 
    {
        Node::operator=(a);
        sid_ = a.sid_;
        return *this;
    }

    u_int16_t sid_; ///< String identifier used by RIB TLV
}; // RIBNode

typedef PointerList<Node> NodeList;
typedef PointerList<RIBNode> RIBNodeList;

}; //namespace prophet

#endif // _PROPHET_NODE_H_

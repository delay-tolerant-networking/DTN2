#ifndef _TOPOLOGY_H_
#define _TOPOLOGY_H_


/**
 * The class that maintains the topology of the network
 */

#include <vector>
#include "debug/Debug.h"
#include "debug/Log.h"

#include "Node.h"
#include "SimContact.h"

class Node;
class SimContact;


class Topology  {

public:
    static void create_node(int id);
    static void create_contact(int id, int src, int dst,  int bw, int delay, int isup, int up, int down);

    static const int MAX_NODES_  = 10;
    static const int MAX_CONTACTS_ = 100;
    
    /**
     * Global configuration of node type.
     * Different node instances are created based upon node type
     * Currently supportes node types:
     * node_type 1 uses SimpleNode
     * node_type 2  uses GlueNode
     */
    static int node_type_ ;
    static int num_cts_ ; 
    static int contacts() { return num_cts_ ; }

   // static std::vector<Node*> nodes_;
    // static std::vector<Contact*> contacts_;

    static Node*  nodes_[];
    static SimContact* contacts_[];
   
    static Node* node(int i) { return Topology::nodes_[i] ; }
    static SimContact* contact(int i) { return Topology::contacts_[i] ; }

};

#endif /* _TOPOLOGY_H_ */

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

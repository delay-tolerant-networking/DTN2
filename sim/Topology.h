#ifndef _TOPOLOGY_H_
#define _TOPOLOGY_H_


/**
 * The class that maintains the topology of the network
 */

#include <vector>
#include "debug/Debug.h"
#include "debug/Log.h"

#include "Node.h"
#include "Contact.h"

class Node;
class Contact;

class Topology : public Logger {

public:
    static void create_node(int id);
    static void create_contact(int id, int src, int dst,  int bw, int delay, int isup, int up, int down);

    static const int MAX_NODES_  = 10;
    static const int MAX_CONTACTS_ = 100;
    
    static int nocts_ ; 
    static int contacts() { return nocts_ ; }

   // static std::vector<Node*> nodes_;
    // static std::vector<Contact*> contacts_;

    static Node*  nodes_[];
    static Contact* contacts_[];
   
    static Node* node(int i) { return Topology::nodes_[i] ; }
    static Contact* contact(int i) { return Topology::contacts_[i] ; }

};

#endif /* _TOPOLOGY_H_ */

#include "Topology.h"

Node* Topology::nodes_[MAX_NODES_];
Contact* Topology::contacts_[MAX_CONTACTS_];

int Topology::nocts_ = 0;

void
Topology::create_node(int id)
{
   Node* nd = new Node(id);
   Topology::nodes_[id] = nd;
}


void
Topology::create_contact(int id, int src, int dst, int bw, int delay, int isup,  int up, int down) 
{
    Contact* ct = new Contact(id,Topology::nodes_[src],Topology::nodes_[dst],bw,delay,isup, up,down);
    Topology::contacts_[id] = ct;
    nocts_++;
}


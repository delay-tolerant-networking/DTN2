#include "Topology.h"
#include "GlueNode.h"
#include "SimpleNode.h"
#include "SimConvergenceLayer.h"


Node* Topology::nodes_[MAX_NODES_];
SimContact* Topology::contacts_[MAX_CONTACTS_];

int Topology::num_cts_ = 0;
int Topology::node_type_ = 1;


void
Topology::create_node(int id)
{
    
    Node* nd;
    
    if (Topology::node_type_ == 1) {
	nd = new SimpleNode(id,"/sim/snode");
    } else if (Topology::node_type_ == 2) {
	nd = new GlueNode(id,"/sim/gluenode");
    } else {
	//logf("/sim/topology",LOG_CRIT, "unimplemented node type");
    }

   Topology::nodes_[id] = nd;
}


void
Topology::create_contact(int id, int src, int dst, int bw, int delay, int isup,  int up, int down) 
{
    // create DTN2 contact
    if (Topology::node_type_ == 2) {
	SimConvergenceLayer::create_ct(id);
    }
    
    SimContact* simct = new SimContact(id,Topology::nodes_[src],Topology::nodes_[dst],bw,delay,isup, up,down);
    Topology::contacts_[id] = simct;
    num_cts_++;


    
}


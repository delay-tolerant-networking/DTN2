#include "Node.h"
#include "Topology.h"


long Node::total_=0;

Node::Node( int id, const char* logpath) : Logger(logpath)
{
    id_ = id;
    if (id_ == -1) {
	id_ = (int)Node::next();
    }

}
Node::~Node() {}



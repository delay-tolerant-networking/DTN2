#ifndef _SIMPLE_NODE_H_
#define _SIMPLE_NODE_H_

#include "Node.h"

/*
 * A simple node definition that performs random routing
 */

class SimpleNode : public Node {

public:

    SimpleNode(int id, const char* logpath);
    virtual ~SimpleNode();
    
    /**
     * Virtual functions from Node
     */

    virtual void process(Event *e); 
    virtual  void chewing_complete(SimContact* c, double size, Message* msg);
    virtual  void open_contact(SimContact* c);
    virtual  void close_contact(SimContact* c);
    virtual  void message_received(Message* msg);
    
    
private:
    virtual  void forward(Message* msg);
    std::vector<Message*> msgq_; /// message to be forwarded

};


#endif /* _SIMPLE_NODE_H_ */

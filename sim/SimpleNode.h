
#ifndef _SIMPLE_NODE_H_
#define _SIMPLE_NODE_H_

#include "Node.h"

/*
 * A simple node definition that performs random routing
 */

class SimpleNode : public Node {

public:
    static long next() {
	return total_ ++ ;
    }

    SimpleNode(int id, const char* logpath);
    virtual ~SimpleNode();
    
    virtual void process(Event *e); ///< virtual function from Processable

    
    virtual  void chewing_complete(SimContact* c, double size, Message* msg);
    virtual  void open_contact(SimContact* c);
    virtual  void close_contact(SimContact* c);
    virtual  void message_received(Message* msg);
    virtual  void forward(Message* msg);
    
private:
    std::vector<Message*> msgq_;

};


#endif /* _SIMPLE_NODE_H_ */

#ifndef _GLUE_NODE_H_
#define _GLUE_NODE_H_

/*
 * Interface to DTN2  and Simulator
 */


#include "Node.h"
#include "routing/BundleRouter.h"

class GlueNode : public Node {

public:


    GlueNode(int id, const char* logpath);
    virtual void process(Event *e); ///< virtual function from Processable
    
    
    virtual  void chewing_complete(SimContact* c, double size, Message* msg);
    virtual  void open_contact(SimContact* c);
    virtual  void close_contact(SimContact* c);
    virtual  void message_received(Message* msg);
    virtual  void forward(Message* msg);
   
    
private:
    void execute_router_action(BundleAction* action);
    void forward_event(BundleEvent* event) ;
    BundleRouter* router_;
};


#endif /* _GLUE_NODE_H_ */


#ifndef _NODE2_H_
#define _NODE2_H_

/*
 * Interface to DTN2  and Simulator
 */


#include "Node.h"



class GlueNode : public Node {

public:

    GlueNode(int id);
    virtual void process(Event *e); ///< virtual function from Processable

    
   virtual  void chewing_complete(Contact* c, double size, Message* msg);
   virtual  void open_contact(Contact* c);
   virtual  void close_contact(Contact* c);
   virtual  void forward(Message* msg);
    
private:

};

   
#endif /* _NODE2_H_ */


#ifndef _NODE2_H_
#define _NODE2_H_

/*
 * Interface to DTN2  and Simulator
 */


#include "Node.h"



class GlueNode : public Processable, public Logger {

public:
    static long next() {
	return total_ ++ ;
    }

    
    Node(int id);
    virtual ~Node();

    virtual void process(Event *e); ///< virtual function from Processable
    int id() { return id_ ;}

    
   virtual  void chewing_complete(Contact* c, double size, Message* msg);
   virtual  void open_contact(Contact* c);
   virtual  void close_contact(Contact* c);
   virtual  void forward(Message* msg);
    
private:
    static long total_;
    int id_;
    std::vector<Message*> msgq_;
    //   BundleTuple name_;
};

   
#endif /* _NODE2_H_ */

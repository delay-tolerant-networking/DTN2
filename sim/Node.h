
#ifndef _NODE_H_
#define _NODE_H_

/*
 * Interface to DTN Nodes
 */

#include <vector>

#include "Event.h"
#include "debug/Debug.h"
#include "debug/Log.h"
#include "Processable.h"
#include "Contact.h"
#include "Message.h"

class Contact;
class Node : public Processable, public Logger {

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

   
#endif /* _NODE_H_ */

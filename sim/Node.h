
#ifndef _NODE_H_
#define _NODE_H_

/*
 * Abstract class defining structure of a node in simulator
 */

#include <vector>

#include "Event.h"
#include "debug/Debug.h"
#include "debug/Log.h"
#include "Processable.h"
#include "SimContact.h"
#include "Message.h"

class SimContact;
class Node : public Processable, public Logger {

public:
    static long next() {
	return total_ ++ ;
    }

    Node(int id, const char* logpath);
    virtual ~Node();

    int id() { return id_ ;}
    virtual void process(Event *e) = 0; ///< virtual function from Processable
    
    virtual  void chewing_complete(SimContact* c, double size, Message* msg) = 0;
    virtual  void open_contact(SimContact* c) = 0;
    virtual  void close_contact(SimContact* c) = 0;
    virtual  void message_received(Message* msg) = 0;
    virtual  void forward(Message* msg) = 0;
    
protected:
    static long total_;
    int id_;
   
};


#endif /* _NODE_H_ */

#ifndef _NODE_H_
#define _NODE_H_

/*
 * Abstract class defining structure of a node in simulator
 */

#include <vector>

#include "debug/Debug.h"
#include "debug/Log.h"
#include "Processable.h"
#include "SimContact.h"
#include "Message.h"

class SimContact;


class Node : public Processable, public Logger {

public:

    /**
     * Constructor / Destructor
     */
    Node(int id, const char* logpath) 
	: Logger(logpath),id_(id) {}
    virtual ~Node() {}
        
    int id() { return id_ ;}
    
    /**
     * Virtual function from processable
     */
    virtual void process(Event *e) = 0; 
    
    /**
     * Action when informed that message transmission is finished.
     * Invoked by SimContact
     */
    virtual  void chewing_complete(SimContact* c, double size, Message* msg) = 0;
    
    /**
     * Action when a contact opens/closes
     * Invoked by SimContact
     */
    virtual  void open_contact(SimContact* c) = 0;
    virtual  void close_contact(SimContact* c) = 0;

    /**
     * Action when the MESSAGE_RECEIVED event arrives
     */
    virtual  void message_received(Message* msg) = 0;
    
    virtual  void create_consumer() {};
    
protected:
    int id_;
   
};


#endif /* _NODE_H_ */

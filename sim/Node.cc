#include "Node.h"
#include "Topology.h"


long Node::total_=0;

Node::Node( int id) : Logger("/sim/node"),msgq_(0)
{
    id_ = id;
    if (id_ == -1) {
	id_ = (int)Node::next();
    }

}
Node::~Node() {}

void 
Node::chewing_complete(Contact* c, double size, Message* msg) {
    
    if (msg->size() != size){
	log_info("ChXXX for msg %d, frac %f",msg->id(),size*1.0/msg->size());
	if (size != 0) {
	    msg = (Message* )msg->clone();
	    msg->rm_bytes(size);
	}
	forward(msg);
    }
    else {
	log_info("ChC  for msg %d",msg->id());
    }
}

bool valid_contact(Contact* c, Message* m) {
    if (m->src() == c->dst()->id()) return false;
    return true;
}

void 
Node::open_contact(Contact* ct) {

    for(u_int i=0;i<msgq_.size();i++) {
	Message* msg =  msgq_[i];
	if (valid_contact(ct,msg)) {

	    log_debug("contact is open and valid for messaged %d",msg->id());
	    std::vector<Message*>::iterator rmv = msgq_.begin() + i - 1;
	    
	    // now, actually remove this element from the array
	    msgq_.erase(rmv);
	    ct->chew_message(msg);
	    return;
	}
    }

}

void 
Node::close_contact(Contact* c) {
// Do nothing
}

void
Node::forward(Message* msg) {


    for(int i=0;i<Topology::contacts();i++){
	Contact *c = Topology::contact(i);
	if (c->is_open() && (c->src()->id() == id() )) {
	   
	    if (valid_contact(c,msg)) {
		c->chew_message(msg);
		return;
	    }
	}
    }
    /* No one ate the message, so store it */
    
    msgq_.push_back(msg);
    

}
   

void
Node::process(Event* e) {

    
    if (e->sameTypeAs(APP_MESSAGE_ARRIVAL) ||
	(e->sameTypeAs(CONTACT_MESSAGE_ARRIVAL))) {
	
	Message* msg;
	if (e->sameTypeAs(APP_MESSAGE_ARRIVAL)) {
	    Event_app_message_arrival* e1 = ((Event_app_message_arrival*) e);
	    msg = ((Event_app_message_arrival* )e1)->msg_;
	    log_info("message from own tragent msg (%d) size %3f",msg->id(),msg->size());
	}
	else {
	    Event_contact_message_arrival* e1 = ((Event_contact_message_arrival*) e);
	    msg = e1->msg_;
	    double newsize = e1->sizesent_;
	    msg->set_size(newsize);
	    // can also change the size of the message when we detect that this is the problem
	    // i.e when we schedule the event
	}

	if (msg->dst() == id()) {
	    log_info("Rc msg at node %d, id %d, size-rcv %f",id(),msg->id(),msg->size());
	}
	else {
	    log_info("Fw msg at node %d, id %d, size-rcv %f",id(),msg->id(),msg->size());
	    forward(msg);
	}
    }
    
    else
	PANIC("unimplemented action code");
  

}

#include "GlueNode.h"

GlueNode::GlueNode(int id): Node(id) 
{

    BundleRouter* daemon = new BundleRouter();
    
}


void 
process(Event *e) {

    if (e->type() == APP_MESSAGE_ARRIVAL) {
	
	Event_app_message_arrival* e1 = ((Event_app_message_arrival*) e);
	msg = ((Event_app_message_arrival* )e1)->msg_;
	log_info("message from own tragent msg (%d) size %3f",msg->id(),msg->size());

	// Bundle *b = new Bundle(src,dst,size)
	// Do address translations for src, dst
	// may be set ports etc.
	Bundle *b = new Bundle() // temporary
	    daemon->exec_event(BUNDLE_ARRIVED
    }

}


void 
chewing_complete(Contact* c, double size, Message* msg);

void 
open_contact(Contact* c);


void 
close_contact(Contact* c);


void 
forward(Message* msg);

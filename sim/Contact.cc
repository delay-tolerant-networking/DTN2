#include "Contact.h"
#include "Simulator.h"



bool Contact::ALLOW_FRAGMENTATION = true;
bool Contact::DISALLOW_FRACTIONAL_TRANSFERS = true;
long Contact::total_ = 0;


Contact::Contact (int id, Node* src, Node* dst, double bw, double latency, bool isup, int pup, int pdown)   : Logger ("/sim/contact") 
{
    id_ = id;
    if (id_ == -1) {
	id_ = (int)Contact::next();
    }
    src_ = src;
    dst_ = dst;
    bw_ = bw;
    latency_ = latency;
    up_ = pup;
    down_ = pdown;
    ASSERT(bw > 0 && latency >= 0) ;
    


    chewing_event_ = NULL;
    future_updown_event_ = NULL;
    // start state
    if (isup) {
	open_contact(false);
    } else  {
	close_contact(false);
    }
    
    log_debug("contact created with id %d between nodes (%d,%d)...\n",id_,src_->id(),dst_->id());
}


// this defines behavior of starting of eating of a message
void 
Contact::chew_message(Message* msg) {

        log_info("ChSt for bundle %d", msg->id());

        ASSERT(state_ == OPEN);
        state_ = BUSY;

        double tr = msg->size()/bw_;

	log_debug("Trying to chew a message (%d) of size %f",msg->id(),msg->size());
	ASSERT(tr >0);
	double tmp = Simulator::time() + tr ; 
        Event_contact_chewing_finished* e2 = new Event_contact_chewing_finished(tmp,this);
        e2->msg_ = msg;
        e2->chew_starttime_ = Simulator::time() ;
        chewing_event_ = e2;
	Simulator::add_event(e2);
}


bool
Contact::is_open() {
    return state_ == OPEN ;
}



void 
Contact::chewing_complete(double size, Message* msg) {

    if (size > msg->size()) {
	log_debug(" transmit size exceeding message (%d) size (%2f > %2f) "
		  ,msg->id(),size,msg->size());

	size = msg->size();
    }

    if (state_ == CLOSE)
	return;

    if (size != 0) {
  	Event_contact_message_arrival* e = new Event_contact_message_arrival(Simulator::time()+latency_,dst_);
	e->source_contact_ = this;
	e->msg_ = msg ;
	e->sizesent_ = size;
	Simulator::add_event(e);
    }
    src_->chewing_complete(this,size,msg);
}


void 
remove_event(Event* e) 
{
    if (e != NULL)
	Simulator::remove_event(e);
}


void
Contact::open_contact(bool forever) 
{

     
    remove_event(future_updown_event_);
    future_updown_event_ = NULL;

    // if it is a regular scheduled open, schedule a future down event
    if (!forever) {
	// schedule a link down event
	double next = up_;
	Event_contact_down* e = new Event_contact_down(Simulator::time()+next,this);
	Simulator::add_event(e);
	future_updown_event_ = e;
    }
    
    state_ = OPEN;
    // send message to src, telling about that the contact is available
    src_->open_contact(this);
}



void 
Contact::close_contact(bool forever) {
    
    remove_event(future_updown_event_);
    future_updown_event_ = NULL;
    if (!forever) {
	// schedule a link up event
	double next = down_;
	Event_contact_up* e = new Event_contact_up(Simulator::time()+next,this);
	Simulator::add_event(e);
	future_updown_event_ = e;
    }
    
    
    if (state_ == BUSY) {
	
	ASSERT(chewing_event_ != NULL);
	// schedule partial chewing complete
	

	double transmit_time = Simulator::time() - chewing_event_->chew_starttime_;
	double size_transmit = (bw_*transmit_time);
	
	if (!Contact::ALLOW_FRAGMENTATION) {
	    size_transmit = 0;
	} else if (Contact::DISALLOW_FRACTIONAL_TRANSFERS) {
	    size_transmit = (int)(size_transmit);
	}
	chewing_complete(size_transmit,chewing_event_->msg_);
	remove_event(chewing_event_);
	chewing_event_ = NULL;
	// tell the source that this contact is closed
	src_->close_contact(this);
    }
    state_ = CLOSE;
}


void 
Contact::process(Event* e) {

    
        if (e->sameTypeAs(CONTACT_UP)) {
            open_contact(((Event_contact_up* )e)->forever_);
        } else if  (e->sameTypeAs(CONTACT_DOWN)) {
            close_contact(((Event_contact_down* )e)->forever_);
        } else if  (e->sameTypeAs(CONTACT_CHEWING_FINISHED)) {

            if (state_ == CLOSE)  return;
	    state_ = OPEN;
	    Event_contact_chewing_finished* e1 = (Event_contact_chewing_finished* )e;
            chewing_complete( e1->msg_->size(),e1->msg_);
            chewing_event_ = NULL;
            src_->open_contact(this);

        } else {
            PANIC("undefined event \n");
        }
}





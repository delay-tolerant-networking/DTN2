#include "SimContact.h"
#include "Simulator.h"



bool SimContact::ALLOW_FRAGMENTATION = true;
bool SimContact::DISALLOW_FRACTIONAL_TRANSFERS = true;
long SimContact::total_ = 0;


SimContact::SimContact (int id, Node* src, Node* dst, double bw, double latency, bool isup, int pup, int pdown)   : Logger ("/sim/contact") 
{
    id_ = id;
    if (id_ == -1) {
	id_ = (int)SimContact::next();
    }
    src_ = src;
    dst_ = dst;
    bw_ = bw;
    latency_ = latency;
    up_ = pup;
    down_ = pdown;
    ASSERT(bw > 0 && latency >= 0) ;
    
    log_info("Contact created with id %d between nodes (%d,%d)...\n",id_,src_->id(),dst_->id());

    chewing_event_ = NULL;
    future_updown_event_ = NULL;
    // start state
    if (isup) {
	open_contact(false);
    } else  {
	close_contact(false);
    }
    
 
}


// this defines behavior of starting of eating of a message
void 
SimContact::chew_message(Message* msg) 
{
    log_info("ChSt for message %d", msg->id());
    
    ASSERT(state_ == OPEN);
    state_ = BUSY;
    double tr = msg->size()/bw_;
        ASSERT(tr >0);
    double tmp = Simulator::time() + tr ; 
    Event_contact_chewing_finished* e2 = new Event_contact_chewing_finished(tmp,this,msg,Simulator::time());
    chewing_event_ = e2;
    Simulator::add_event(e2);
}


bool
SimContact::is_open() 
{
    return state_ == OPEN ;
}



void 
SimContact::chewing_complete(double size, Message* msg) 
{
    
    log_info("ChC for message %d, orig size %2f, transmitted %2f", msg->id(),msg->size(),size);
    if (size > msg->size()) {
	log_debug(" transmit size exceeding message (%d) size (%2f > %2f) "
		  ,msg->id(),size,msg->size());
	
	size = msg->size();
    }
    
    if (state_ == CLOSE)
	return;
    
    if (size != 0) {
  	Event_message_received* e = new Event_message_received(Simulator::time()+latency_,dst_,size,this,msg);
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
SimContact::open_contact(bool forever) 
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
SimContact::close_contact(bool forever) 
{
    
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
	
	if (!SimContact::ALLOW_FRAGMENTATION) {
	    size_transmit = 0;
	} else if (SimContact::DISALLOW_FRACTIONAL_TRANSFERS) {
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
SimContact::process(Event* e) 
{
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






#include "TrAgent.h"
#include "Message.h"
#include "Simulator.h"
#include "Node.h"

TrAgent::TrAgent(double t,int src, int dst, int size, int batchsize, int reps, int gap) 
: Logger ("/sim/tragent") {
    start_time_ = t;
    src_ = src;
    dst_= dst;
    reps_ = reps;
    batchsize_ = batchsize;
    gap_ = gap;
    size_ = size;
    repsdone_ = 0;
  
}

void 
TrAgent::start() {
    Event* e1 = new Event(start_time_,this,TR_NEXT_SENDTIME);
    Simulator::add_event(e1);
}

void
TrAgent::send(double time, int size) {

    Message* msg = new Message(src_,dst_,size);
    Node* src_tmp = Topology::node(src_);
    Event_message_received *esend = new Event_message_received(time,src_tmp,size,NULL,msg);
    Simulator::add_event(esend);
    log_info("G msg id %d size %d",msg->id(),size);
}

void
TrAgent::process(Event* e) {
    
    if (e->type() == TR_NEXT_SENDTIME) {
	for(int i=0;i<batchsize_;i++) {
	    send(Simulator::time(),size_);
	}
            
	// Increment # of reps
	repsdone_ ++;
	if (repsdone_ < reps_) {
	    double tmp = Simulator::time() + gap_ ;
	    Event* e1 = new Event(tmp,this,TR_NEXT_SENDTIME);
	    Simulator::add_event(e1);
	} 
	else {
	    log_info("All bacthes finished");
	}
    }
    else {
	log_debug("undefined event");
    }
}



/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _EVENT_H_
#define _EVENT_H_


#include "Processable.h"
#include "Topology.h"
#include "Node.h"
#include "Message.h"

#include "bundling/BundleEvent.h"

class SimContact;
class Node;
class Message;
class Processable;


/******************************************************************************
 *
 * Event Types
 *
 *****************************************************************************/


typedef enum {
    MESSAGE_RECEIVED = 0x1,     ///< New message arrival

    SIM_CONTACT_UP,                 ///< SimContact is available

    SIM_CONTACT_DOWN,               ///< SimContact abnormally terminated

    CONTACT_CHEWING_FINISHED,   ///< Message transmission finished

    TR_NEXT_SENDTIME  ,         ///< Used by traffic agent to send data periodically
    
    FOR_BUNDLE_ROUTER ,         ///< A bundle event to be forwared to bundle router
    SIM_PRINT_STATS  ,          ///< Simulator print stats
} sim_event_type_t;



/**
 * Pretty printer function simulation events.
 */
static const char* 
ev2str(sim_event_type_t eventtype) {
    switch (eventtype) {
    case MESSAGE_RECEIVED:  return "MESSAGE_RECEIVED";
    case SIM_CONTACT_UP:        return "SIM_CONTACT_UP";
    case SIM_CONTACT_DOWN:      return "SIM_CONTACT_DOWN";
    case CONTACT_CHEWING_FINISHED: return "CONTACT_CHEWING_FINISHED";
    case TR_NEXT_SENDTIME:  return "TR_NEXT_SENDTIME";
    case FOR_BUNDLE_ROUTER: return "FOR_BUNDLE_ROUTER";
    case SIM_PRINT_STATS:   return "SIM_PRINT_STATS";
    default:                return "_UNKNOWN_EVENT_";
    }
}


/******************************************************************************
 *
 * Base Event Class
 *
 *****************************************************************************/



class Event {
public:

/**
 * Constructor 
 */
    Event (double time, Processable *handler, sim_event_type_t eventcode);

    
/**
 * Destructor 
 */
    ~Event () {}
    

    Processable* handler(){ return handler_; }  ///> The entity that will process event
    double time()         { return time_ ;   }  ///> Time at which it fires
    bool is_valid()       {  return valid_;  }  ///> Check if it is canceled or not

    void cancel() ;


    sim_event_type_t type() { return type_ ; }
    const char* type_str()  { return ev2str(type_); }
    
private:
    double time_;       
    Processable* handler_;
    sim_event_type_t type_;
    bool valid_;
};




/******************************************************************************
 *
 * EventCompare
 *
 *****************************************************************************/


class EventCompare {

public:
    inline bool operator ()(Event* a, Event* b);
};

/**
 * The comparator function used in the priority queue.
 */
bool
EventCompare::operator()(Event* a, Event* b)
{
    return a->time() >  b->time() ;
}



/******************************************************************************
 *
 * Specific Event Types
 *
 *****************************************************************************/

/*******************************************************************
 *
 * Events for SimContact
 *
 ******************************************************************/

class Event_contact_up : public Event {
    
public:
    
    Event_contact_up(double t, Processable* h) 
	: Event(t,h,SIM_CONTACT_UP),forever_(false) {}
    
    bool forever_;
};



class Event_contact_down : public Event {
    public:
    
    Event_contact_down(double t, Processable* h) 
	: Event(t,h,SIM_CONTACT_DOWN),forever_(false) {}
    bool forever_;
};


class Event_chew_fin: public Event {
public:
    Event_chew_fin(double t, Processable* h,Message* msg, double chewt) 
	: Event (t,h,CONTACT_CHEWING_FINISHED),msg_(msg),chew_starttime_(chewt) {}
    
    Message* msg_;                        ///> message under consideration 
    double chew_starttime_;               ///> the time at which chewing started
    
};


/*******************************************************************
 *
 * Events for Node
 *
 ******************************************************************/

class Event_message_received : public Event {
public:
    
    Event_message_received(double t, Processable* h, 
			   double sizesent, SimContact* ct, Message* msg)
	: Event(t,h,MESSAGE_RECEIVED),sizesent_(sizesent),frm_(ct),msg_(msg) {}
    
    double  sizesent_;              ///> the amount of message that was sent 
    SimContact* frm_;               ///> who sent it, NULL for app
    Message* msg_;                  ///> the received messsage
};



/**
 * Event for Glue Node. The event is actually forwarded
 * to BundleRouter
 */

class Event_for_br: public Event {
public:
    Event_for_br(double t, Processable* h,BundleEvent* e) 
	: Event (t,h,FOR_BUNDLE_ROUTER),bundle_event_(e) {}
    
    BundleEvent* bundle_event_;           ///> event to beforwarded further
};



/**
 * Print Stats - goes to the Simulator
 */

class Event_print_stats: public Event {
public:
    Event_print_stats(double t, Processable* h) 
	: Event (t,h,SIM_PRINT_STATS) {}
    
};


#endif /* _EVENT_H_ */

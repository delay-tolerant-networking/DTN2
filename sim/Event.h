#ifndef _EVENT_H_
#define _EVENT_H_


#include "Processable.h"
#include "SimContact.h"
#include "Topology.h"
#include "Node.h"
#include "Message.h"

#include "bundling/BundleEvent.h"

class SimContact;
class Message;
class Processable;



/******************************************************************************
 *
 * Event Types
 *
 *****************************************************************************/


typedef enum {
    MESSAGE_RECEIVED = 0x1,      ///< New bundle arrival
    //  BUNDLE_TRANSMITTED,         ///< Bundle or fragment successfully sent

    CONTACT_UP,                 ///< SimContact is available
    CONTACT_DOWN,               ///< SimContact abnormally terminated

    CONTACT_CHEWING_FINISHED,   ///< Message transmission finished
//    CONTACT_MESSAGE_ARRIVAL,    ///< Message received via contact

//    APP_MESSAGE_ARRIVAL,        ///< Message has arrived from app
    TR_NEXT_SENDTIME  ,          ///< Used by the traffic agent to schedule event for itself
    
    FOR_BUNDLE_ROUTER             ///< A bundle event to be forwared to bundle router

} sim_event_type_t;



/**
 * Pretty printer function simulation events.
 */
static const char* 
ev2str(sim_event_type_t eventtype) {
    switch (eventtype) {
    case MESSAGE_RECEIVED:          return "MESSAGE_RECEIVED";
//    case BUNDLE_TRANSMITTED:        return "BUNDLE_TRANSMITTED";

    case CONTACT_UP:     return "CONTACT_UP";
    case CONTACT_DOWN:     return "CONTACT_DOWN";

    case CONTACT_CHEWING_FINISHED: return "CONTACT_CHEWING_FINISHED";
//    case CONTACT_MESSAGE_ARRIVAL: return "CONTACT_MESSAGE_ARRIVAL";
	
//    case APP_MESSAGE_ARRIVAL: return "APP_MESSAGE_ARRIVAL";
    case TR_NEXT_SENDTIME: return "TR_NEXT_SENDTIME";

    case FOR_BUNDLE_ROUTER: return "FOR_BUNDLE_ROUTER";

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
    
    Processable* handler() ;
    double time();
    void cancel() ;
    bool is_valid() ;
    bool sameTypeAs(sim_event_type_t t);
    bool sameTypeAs(Event* e);
    sim_event_type_t type() { return type_ ; }
    const char* type_str() { return ev2str(type_); }

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


/*
class Event_app_message_arrival : public Event {
    
public:
    
    Event_app_message_arrival(double t, Processable* h) : Event(t,h,APP_MESSAGE_ARRIVAL) {}
    
    double  origsize_;
    Message* msg_;
};
*/

class Event_contact_up : public Event {
    
public:
    
    Event_contact_up(double t, Processable* h) : Event(t,h,CONTACT_UP),forever_(false) {}
    
    bool forever_;
};



class Event_contact_down : public Event {
    
public:
    
    Event_contact_down(double t, Processable* h) : Event(t,h,CONTACT_DOWN),forever_(false) {}
    bool forever_;
};



class Event_message_received : public Event {
    
public:
    
    Event_message_received(double t, Processable* h, double sizesent, SimContact* ct, Message* msg)
 : Event(t,h,MESSAGE_RECEIVED),sizesent_(sizesent),frm_(ct),msg_(msg) {}
    
    double  sizesent_;              ///> the amount of message that was sent 
    SimContact* frm_;    ///> the contact on which the message was received, if its an app then this is NULL
    Message* msg_;                  ///> the received messsage
};


class Event_contact_chewing_finished : public Event {
    
public:
    Event_contact_chewing_finished(double t, Processable* h,Message* msg, double chewt) : Event (t,h,CONTACT_CHEWING_FINISHED),msg_(msg),chew_starttime_(chewt) {}
    
    Message* msg_; ///> message under consideration 
    double chew_starttime_; ///> the time at which chewing started
    
};


class Event_for_bundle_router: public Event {
    
public:
    Event_for_bundle_router(double t, Processable* h,BundleEvent* e) : Event (t,h,FOR_BUNDLE_ROUTER),bundle_event_(e) {}
    
    BundleEvent* bundle_event_; ///> the actual event that has to be forwarded further
    
};

#endif /* _EVENT_H_ */

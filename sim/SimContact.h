/**
 * Defines and implements link behavior
 */

#ifndef _SIM_CONTACT_H_
#define _SIM_CONTACT_H_


#include "debug/Debug.h"
#include "debug/Log.h"


#include "Node.h"
#include "Processable.h"
#include "Event.h"
#include "Message.h"


class Event;
class Event_chew_fin;

class SimContact : public Logger, public Processable {

public:
    
    static const bool ALLOW_FRAGMENTATION = true;
    static const bool DISALLOW_FRACTIONAL_TRANSFERS = true;
    
    SimContact (int id, Node* src, Node* dst, double bw, 
		double latency, bool isup, int up, int down);
    
    
    
    bool  is_open() ;                ///> returns if the contact is open
    
   /* A message can be sent on a contact only if it is open.
    * The contact itself has no queue. It resembles an actual
    * link
    */
    void chew_message(Message* msg) ; ///> Start transmitting the message

    Node* src() { return src_; }  
    Node* dst() { return dst_; }
    int id() { return id_; }

private:
//    static long total_;
    int id_;
    Node* src_ ;
    Node* dst_ ;
    double latency_;
    double bw_;
    // Probability_Distribution* uptime_; ///< sequence of uptimes
    // Probability_Distribution* downtime_; ///< sequence of downtimes

    int up_; ///< link up time, before the link goes down for down_ time
    int down_;

    typedef enum {
	OPEN = 1, // Available to send a message
	BUSY = 2, // Currently sending a message
	CLOSE = 3 /// Closed
    } state_t;
    
    state_t state_;

    Event_chew_fin* chewing_event_ ; ///> pointer to message currently consumed
    Event* future_updown_event_;     ///> pointer to next up/down event

    void  open_contact(bool b) ;    ///> action to take when contact is open
    void  close_contact(bool b) ;   ///> action to take when contact closes
    void  process(Event* e) ;       ///> Inherited from processable
    void  chewing_complete(double size, Message* msg) ;
};

#endif /* _SIM_CONTACT_H_ */

/**
 * Defines and implements link behavior
 */

#ifndef _CONTACT_H_
#define _CONTACT_H_

#include "Event.h"
#include "Node.h"
#include "Message.h"
#include "debug/Debug.h"
#include "debug/Log.h"

class Event;
class Event_contact_chewing_finished;
class Node;

class Contact : public Logger, public Processable {

public:
    
    static bool ALLOW_FRAGMENTATION;
    static bool DISALLOW_FRACTIONAL_TRANSFERS;
    static long next() {
      return total_ ++ ;
    }
    
    Contact (int id, Node* src, Node* dst, double bw, double latency, bool isup, int up, int down);
    
    
    
    bool  is_open() ;
    void chew_message(Message* msg) ;
    
    Node* src() { return src_; }
    Node* dst() { return dst_; }
    
private:
    static long total_;
    int id_;
    Node* src_ ;
    Node* dst_ ;
    double latency_;
    double bw_;


    // Probability_Distribution* uptime_; ///< sequence of uptimes
    // Probability_Distribution* downtime_; ///< sequence of downtimes
    
    int up_;
    int down_;
    


    void  chewing_complete(double size, Message* msg) ;
    void  open_contact(bool b) ;
    void  close_contact(bool b) ;

    
    void  process(Event* e) ;

    typedef enum {
	OPEN,
	BUSY,
	CLOSE,
    } state_t;
    
    state_t state_;

    Event_contact_chewing_finished* chewing_event_ ;
    Event* future_updown_event_;

    // Messages in pipe not implemented





};

#endif /* _CONTACT_H_ */

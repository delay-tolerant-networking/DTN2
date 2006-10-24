/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _SIM_CONTACT_H_
#define _SIM_CONTACT_H_


#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Log.h>


#include "Node.h"
#include "Processable.h"
#include "SimEvent.h"
#include "Message.h"

namespace dtnsim {


class Event;
class Event_chew_fin;

/**
 * A simulated contact.
 */
class SimContact : public oasys::Logger, public Processable {

public:
    
    static const bool ALLOW_FRAGMENTATION = false;
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
} // namespace dtnsim

#endif /* _SIM_CONTACT_H_ */

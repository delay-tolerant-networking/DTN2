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

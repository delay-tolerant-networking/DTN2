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

#include "bundling/BundleEvent.h"

using namespace dtn;

namespace dtnsim {

class EventHandler;

/******************************************************************************
 *
 * Event Types
 *
 *****************************************************************************/
typedef enum {
    SIM_ROUTER_EVENT = 0x1,	///< Event to be delivered to the router
    
    SIM_CONTACT_UP,		///< SimContact is available
    SIM_CONTACT_DOWN,		///< SimContact abnormally terminated
    SIM_NEXT_SENDTIME,		///< Used by traffic agent to send data
    
} sim_event_type_t;

/**
 * Pretty printer function simulation events.
 */
static const char* 
ev2str(sim_event_type_t event) {
    switch (event) {
    case SIM_ROUTER_EVENT:		return "SIM_ROUTER_EVENT";
    case SIM_CONTACT_UP:		return "SIM_CONTACT_UP";
    case SIM_CONTACT_DOWN:		return "SIM_CONTACT_DOWN";
    case SIM_NEXT_SENDTIME:		return "SIM_NEXT_SENDTIME";
    }

    NOTREACHED;
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
    Event(sim_event_type_t type, int time, EventHandler *handler)
        : type_(type), time_(time), handler_(handler), valid_(true) {}

    EventHandler* handler()  { return handler_; }
    int time()               { return time_ ; }
    bool is_valid()          { return valid_; }
    sim_event_type_t type()  { return type_ ; }
    const char* type_str()   { return ev2str(type_); }

    void cancel()            { valid_ = false; }

private:
    sim_event_type_t type_;	///< Type of the event
    int time_; 		      	///< Time when the event will fire
    EventHandler* handler_;	///< Handler that will process the event
    bool valid_;		///< Indicator if the event was cancelled
};


/******************************************************************************
 *
 * EventCompare
 *
 *****************************************************************************/
class EventCompare {
public:
    /**
     * The comparator function used in the priority queue.
     */
    bool operator () (Event* a, Event* b)
    {
        return a->time() > b->time();
    }
};

/*******************************************************************
 *
 * Events for Node
 *
 ******************************************************************/

class SimRouterEvent : public Event {
public:
    SimRouterEvent(int time, EventHandler* handler, BundleEvent* event)
        
	: Event(SIM_ROUTER_EVENT, time, handler), event_(event) {}
    
    BundleEvent* event_;
};

} // namespace dtnsim

#endif /* _EVENT_H_ */

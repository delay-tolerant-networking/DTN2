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
#ifndef _DTN_SIM_EVENT_H_
#define _DTN_SIM_EVENT_H_

#include "bundling/BundleEvent.h"

using namespace dtn;

namespace dtnsim {

class SimEventHandler;

/******************************************************************************
 *
 * Event Types
 *
 *****************************************************************************/
typedef enum {
    SIM_ROUTER_EVENT = 0x1,	///< Event to be delivered to the router
    SIM_ADD_LINK,		///< Link added
    SIM_DEL_LINK,		///< Link deleted
    SIM_ADD_ROUTE,		///< Route added
    SIM_DEL_ROUTE,		///< Route deleted
    SIM_CONTACT_UP,		///< SimContact is available
    SIM_CONTACT_DOWN,		///< SimContact abnormally terminated
    SIM_NEXT_SENDTIME,		///< Used by traffic agent to send data
    
} sim_event_type_t;

/**
 * Pretty printer function simulation events.
 */
static const char* 
sim_ev2str(sim_event_type_t event) {
    switch (event) {
    case SIM_ROUTER_EVENT:		return "SIM_ROUTER_EVENT";
    case SIM_ADD_LINK:			return "SIM_ADD_LINK";
    case SIM_DEL_LINK:			return "SIM_DEL_LINK";
    case SIM_ADD_ROUTE:			return "SIM_ADD_ROUTE";
    case SIM_DEL_ROUTE:			return "SIM_DEL_ROUTE";
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
class SimEvent {
public:
    /**
     * Constructor 
     */
    SimEvent(sim_event_type_t type, int time, SimEventHandler *handler)
        : type_(type), time_(time), handler_(handler), valid_(true) {}

    SimEventHandler* handler()  { return handler_; }
    int time()               { return time_ ; }
    bool is_valid()          { return valid_; }
    sim_event_type_t type()  { return type_ ; }
    const char* type_str()   { return sim_ev2str(type_); }

    void cancel()            { valid_ = false; }

private:
    sim_event_type_t type_;	///< Type of the event
    int time_; 		      	///< Time when the event will fire
    SimEventHandler* handler_;	///< Handler that will process the event
    bool valid_;		///< Indicator if the event was cancelled
};


/******************************************************************************
 *
 * SimEventCompare
 *
 *****************************************************************************/
class SimEventCompare {
public:
    /**
     * The comparator function used in the priority queue.
     */
    bool operator () (SimEvent* a, SimEvent* b)
    {
        return a->time() > b->time();
    }
};

/*******************************************************************
 *
 * SimRouterEvent -- catch all event class to wrap delivering an event
 * to the bundle router at a particular time.
 *
 ******************************************************************/
class SimRouterEvent : public SimEvent {
public:
    SimRouterEvent(int time, SimEventHandler* handler, BundleEvent* event)
	: SimEvent(SIM_ROUTER_EVENT, time, handler), event_(event) {}
    
    BundleEvent* event_;
};

/*******************************************************************
 *
 * SimAddLinkEvent
 *
 ******************************************************************/
class SimAddLinkEvent : public SimEvent {
public:
    SimAddLinkEvent(int time, SimEventHandler* handler, Link* link)
	: SimEvent(SIM_ADD_LINK, time, handler), link_(link) {}
    
    Link* link_;
};

/*******************************************************************
 *
 * SimDelLinkEvent
 *
 ******************************************************************/
class SimDelLinkEvent : public SimEvent {
public:
    SimDelLinkEvent(int time, SimEventHandler* handler, Link* link)
	: SimEvent(SIM_DEL_LINK, time, handler), link_(link) {}
    
    Link* link_;
};

/*******************************************************************
 *
 * SimAddRouteEvent
 *
 ******************************************************************/
class SimAddRouteEvent : public SimEvent {
public:
    SimAddRouteEvent(int time, SimEventHandler* handler,
                     const BundleTuplePattern& dest, const char* nexthop)
	: SimEvent(SIM_ADD_ROUTE, time, handler),
          dest_(dest), nexthop_(nexthop) {}
    
    BundleTuplePattern dest_;
    std::string nexthop_;
};

} // namespace dtnsim

#endif /* _DTN_SIM_EVENT_H_ */

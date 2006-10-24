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

#ifndef _DTN_SIM_EVENT_H_
#define _DTN_SIM_EVENT_H_

#include "bundling/BundleEvent.h"

using namespace dtn;

namespace dtnsim {

class ConnState;
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
    SIM_CONNECTIVITY,		///< Event for the connectivity module
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
    case SIM_CONNECTIVITY:		return "SIM_CONNECTIVITY";
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
    SimEvent(sim_event_type_t type, double time, SimEventHandler *handler)
        : type_(type), time_(time), handler_(handler), valid_(true) {}

    SimEventHandler* handler()  { return handler_; }
    double time()               { return time_ ; }
    bool is_valid()          { return valid_; }
    sim_event_type_t type()  { return type_ ; }
    const char* type_str()   { return sim_ev2str(type_); }

    void cancel()            { valid_ = false; }

private:
    sim_event_type_t type_;	///< Type of the event
    double time_;	      	///< Time when the event will fire
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
    SimRouterEvent(double time, SimEventHandler* handler, BundleEvent* event)
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
    SimAddLinkEvent(double time, SimEventHandler* handler, Link* link)
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
    SimDelLinkEvent(double time, SimEventHandler* handler, Link* link)
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
    SimAddRouteEvent(double time, SimEventHandler* handler,
                     const EndpointIDPattern& dest, const char* nexthop)
	: SimEvent(SIM_ADD_ROUTE, time, handler),
          dest_(dest), nexthop_(nexthop) {}
    
    EndpointIDPattern dest_;
    std::string nexthop_;
};

/*******************************************************************
 *
 * SimConnectivityEvent
 *
 ******************************************************************/
class SimConnectivityEvent : public SimEvent {
public:
    SimConnectivityEvent(double time, SimEventHandler* handler,
                         const char* n1, const char* n2, ConnState* state)
    : SimEvent(SIM_CONNECTIVITY, time, handler),
      n1_(n1), n2_(n2), state_(state) {}

    std::string n1_, n2_;
    ConnState* state_;
};

} // namespace dtnsim

#endif /* _DTN_SIM_EVENT_H_ */

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
#include "Connectivity.h"

using namespace dtn;

namespace dtnsim {

class ConnState;
class SimEventHandler;

/******************************************************************************
 * Event Types
 *****************************************************************************/
typedef enum {
    SIM_CONN_EVENT = 0x1,	///< Event for the connectivity module
    SIM_AT_EVENT,		///< Generic event for delayed execution
    
} sim_event_type_t;

/**
 * Pretty printer function simulation events.
 */
static const char* 
sim_ev2str(sim_event_type_t event) {
    switch (event) {
    case SIM_CONN_EVENT:		return "SIM_CONN_EVENT";
    case SIM_AT_EVENT:			return "SIM_AT_EVENT";
    }

    NOTREACHED;
}

/******************************************************************************
 * Base Event Class
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
 * SimEventCompare
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
 * SimConnectivityEvent
 ******************************************************************/
class SimConnectivityEvent : public SimEvent {
public:
    SimConnectivityEvent(double time, SimEventHandler* handler,
                         const char* n1, const char* n2,
                         const ConnState& state)
        : SimEvent(SIM_CONN_EVENT, time, handler),
          n1_(n1), n2_(n2), state_(state) {}

    std::string n1_, n2_;
    ConnState state_;
};

/*******************************************************************
 * SimAtEvent
 ******************************************************************/
class SimAtEvent : public SimEvent {
public:
    SimAtEvent(double time, SimEventHandler* handler,
               const std::string& cmd)
        : SimEvent(SIM_AT_EVENT, time, handler), cmd_(cmd) {}

    std::string cmd_;
};

} // namespace dtnsim

#endif /* _DTN_SIM_EVENT_H_ */

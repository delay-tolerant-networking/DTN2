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

#include "Simulator.h"
#include "Node.h"
#include "Topology.h"

namespace dtnsim {

Simulator* Simulator::instance_; ///< singleton instance

double Simulator::time_ = 0;
double Simulator::runtill_ = -1;

Simulator::Simulator() 
    : Logger("Simulator", "/sim/main"),
      eventq_()
{
}


void
Simulator::post(SimEvent* e)
{
    instance_->eventq_.push(e);
}

void
Simulator::exit() 
{
    ::exit(0);
}

/**
 * The main simulator thread that fires the next event
 */
void
Simulator::run()
{
    oasys::Log* log = oasys::Log::instance();
    log->set_prefix("--");
    
    log_debug("starting event loop...");
    is_running_ = true;

    // first handle all events posted from the configuration
    Topology::NodeTable::iterator iter;

    for (iter =  Topology::node_table()->begin();
         iter != Topology::node_table()->end();
         ++iter)
    {
        Node* node = iter->second;
        node->set_active();
        node->process_bundle_events();
    }

     while(!eventq_.empty()) {
         log->set_prefix("--");
         if (is_running_) {
             SimEvent* e = eventq_.top();
             eventq_.pop();
             /* Move the clock */
             time_ = e->time();
             if (e->is_valid()) {
                 ASSERT(e->handler() != NULL);
                 /* Process the event */
                 log_debug("Event:%p type %s at time %f",
                           e, e->type_str(), time_);
                 e->handler()->process(e);
             }
             if ((Simulator::runtill_ != -1) &&
                 (time_ > Simulator::runtill_)) {
                 log_info("Exiting simulation. "
                          "Current time (%f) > Max time (%f)",
                          time_, Simulator::runtill_);
                 exit();
             }
         } // if is_running_
     }
     log_info("eventq is empty, time is %f", time_);
}

extern "C" {
int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
    (void)tz;
    double now = Simulator::time();
    tv->tv_sec = (long int) now;
    tv->tv_usec = (int) ((now - tv->tv_sec) * 100000.0);
    return 0;
}
}

void
Simulator::process(SimEvent *e)
{
    (void)e;
    NOTIMPLEMENTED;
}

} // namespace dtnsim

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

#include <oasys/tclcmd/TclCommand.h>

#include "Simulator.h"
#include "Node.h"
#include "Topology.h"
#include "bundling/BundleTimestamp.h"
#include "storage/BundleStore.h"
#include "storage/LinkStore.h"
#include "storage/GlobalStore.h"
#include "storage/RegistrationStore.h"

using namespace dtn;

namespace dtnsim {

//----------------------------------------------------------------------
Simulator* Simulator::instance_; ///< singleton instance

double Simulator::time_ = 0;
double Simulator::runtill_ = -1;

//----------------------------------------------------------------------
Simulator::Simulator(DTNStorageConfig* storage_config) 
    : DTNServer("/dtnsim/sim", storage_config), 
      eventq_(),
      store_(0)
{
    Logger::classname_ = "Simulator";

    // override defaults from oasys storage config
    storage_config->type_ = "memorydb";
    storage_config->init_ = true;
    storage_config->tidy_ = true;
    storage_config->tidy_wait_ = 0;
    storage_config->leave_clean_file_ = false;
    storage_config->payload_dir_ = std::string("/tmp/dtnsim_payloads_") +
                                   getenv("USER");
}

//----------------------------------------------------------------------
void
Simulator::post(SimEvent* e)
{	
    instance_->eventq_.push(e);
}

//----------------------------------------------------------------------
void
Simulator::exit() 
{
    ::exit(0);
}

//----------------------------------------------------------------------
int
Simulator::run_node_events()
{
    bool done;
    int next_timer;
    do {
        done = true;
        next_timer = -1;
        
        Topology::NodeTable::iterator iter;
        for (iter =  Topology::node_table()->begin();
             iter != Topology::node_table()->end();
             ++iter)
        {
            Node* node = iter->second;
            node->set_active();
        
            int next = oasys::TimerSystem::instance()->run_expired_timers();
            if (next != -1) {
                if (next_timer == -1) {
                    next_timer = next;
                } else {
                    next_timer = std::min(next_timer, next);
                }
            }
        
            if (node->process_bundle_events()) {
                done = false;
            }
        }
    } while (!done);

    return next_timer;
}

//----------------------------------------------------------------------
void
Simulator::run()
{
    oasys::Log* log = oasys::Log::instance();
    log->set_prefix("--");
    
    log_debug("Starting Simulator event loop...");
    
    while (1) {
        int next_timer_ms = run_node_events();
        double next_timer = (next_timer_ms == -1) ? INT_MAX :
                            time_ + (((double)next_timer_ms) / 1000);
        double next_event = INT_MAX;
        log->set_prefix("--");
        
        SimEvent* e = NULL;
        if (! eventq_.empty()) {
            e = eventq_.top();
            next_event = e->time();
        }
        
        if ((next_timer_ms == -1) && (e == NULL)) {
            log_info("Simulator loop done -- no pending events or timers");
            break;
        }
        else if (next_timer < next_event) {
            time_ = next_timer;
            log_debug("advancing time by %u ms to %f for next timer",
                      next_timer_ms, time_);
        }
        else {
            ASSERT(e != NULL);
            eventq_.pop();
            time_ = e->time();

            if (e->is_valid()) {
                ASSERT(e->handler() != NULL);
                /* Process the event */
                log_debug("Event:%p type %s at time %f",
                          e, e->type_str(), time_);
                e->handler()->process(e);
            }
        }
        
        if ((Simulator::runtill_ != -1) &&
            (time_ > Simulator::runtill_)) {
            log_info("Exiting simulation. "
                     "Current time (%f) > Max time (%f)",
                     time_, Simulator::runtill_);
            break;
        }

    }
    log_info("eventq is empty, time is %f", time_);
}

//----------------------------------------------------------------------
void
Simulator::pause()
{
    oasys::StaticStringBuffer<128> cmd;
    cmd.appendf("puts \"Simulator paused at time %f...\"", time_);
    oasys::TclCommandInterp::instance()->exec_command(cmd.c_str());
    
    oasys::TclCommandInterp::instance()->exec_command(
        "simple_command_loop \"dtnsim% \"");
}

//----------------------------------------------------------------------
/**
 * Override gettimeofday to return the simulator time.
 */
extern "C" int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
    (void)tz;
    double now = Simulator::time();
    DOUBLE_TO_TIMEVAL(now, *tv);
    return 0;
}

//----------------------------------------------------------------------
void
Simulator::process(SimEvent *e)
{
    switch (e->type()) {
    case SIM_AT_EVENT:
        oasys::TclCommandInterp::instance()->
            exec_command(((SimAtEvent*)e)->cmd_.c_str());
        break;
        
    default:
        NOTREACHED;
    }
}

} // namespace dtnsim

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
#include "bundling/BundleTimestamp.h"
#include "storage/BundleStore.h"
#include "storage/LinkStore.h"
#include "storage/GlobalStore.h"
#include "storage/RegistrationStore.h"

using namespace dtn;

namespace dtnsim {

Simulator* Simulator::instance_; ///< singleton instance

double Simulator::time_ = 0;
double Simulator::runtill_ = -1;


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
    storage_config->payload_dir_ = "/tmp/dtnsim_payloads";
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
    
    log_debug("Starting Simulator event loop...");
    is_running_ = true;

    log_debug("Handling events posted from the configuration...");
    Topology::NodeTable::iterator iter;

    for (iter =  Topology::node_table()->begin();
         iter != Topology::node_table()->end();
         ++iter)
    {
        Node* node = iter->second;
        node->set_active();
        node->process_bundle_events();
    }
	
    log->set_prefix("--");
    log_debug("Entering the event loop...");
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
                break;
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

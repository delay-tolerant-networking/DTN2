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
#include "Simulator.h"
#include "Node.h"
#include "Topology.h"

namespace dtnsim {

Simulator* Simulator::instance_; ///< singleton instance

double Simulator::runtill_ = -1;

Simulator::Simulator() 
    : Logger ("/sim/main"),
      eventq_()
 {
    time_ = 0;
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
        if (is_running_) {
            SimEvent* e = eventq_.top();
            eventq_.pop();
            /* Move the clock */
            time_ =  e->time();
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


void
Simulator::process(SimEvent *e)
{
    NOTIMPLEMENTED;
}

} // namespace dtnsim

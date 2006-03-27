/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
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

#include "Connectivity.h"
#include "Node.h"
#include "SimEvent.h"
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>

namespace dtnsim {

Connectivity* Connectivity::instance_(NULL);
std::string   Connectivity::type_("");

Connectivity::Connectivity()
    : Logger("Connectivity", "/sim/conn")
{
}

Connectivity*
Connectivity::create_conn()
{
    ASSERT(type_ != "");

    if (type_ == "static") {
        // just the base class
        return new Connectivity(); 
    } else {
        __log_crit("/connectivity", "invalid connectivity module type %s",
                   type_.c_str());
        return NULL;
    }
}

/**
 * Utility function to parse a bandwidth specification.
 */
bool
ConnState::parse_bw(const char* bw_str, int* bw)
{
    char* end;
    *bw = 0;
    *bw = strtoul(bw_str, &end, 10);

    if (end == bw_str)
        return false;

    if (*end == '\0') { // no specification means straight bps
        return true;

    } else if (!strcmp(end, "bps")) {
        return true;

    } else if (!strcmp(end, "kbps")) {
        *bw = *bw * 1000;
        return true;

    } else if (!strcmp(end, "Mbps")) {
        *bw = *bw * 1000000;
        return true;

    } else {
        return false;
    }
}

/**
 * Utility function to parse a time specification.
 */
bool
ConnState::parse_time(const char* time_str, int* time)
{
    char* end;
    *time = 0;
    *time = strtoul(time_str, &end, 10);

    if (end == time_str)
        return false;

    if (*end == '\0') { // no specification means ms
        return true;

    } else if (!strcmp(end, "ms")) {
        return true;

    } else if (!strcmp(end, "s")) {
        *time = *time * 1000;
        return true;

    } else if (!strcmp(end, "min")) {
        *time = *time * 1000 * 60;
        return true;

    } else if (!strcmp(end, "hr")) {
        *time = *time * 1000 * 3600;
        return true;

    } else {
        return false;
    }
}

/**
 * Utility function to fill in the values from a set of options
 * (e.g. bw=10kbps, latency=10ms).
 */
bool
ConnState::parse_options(int argc, const char** argv, const char** invalidp)
{
    oasys::OptParser p;
    std::string bw_str;
    std::string latency_str;

    p.addopt(new oasys::StringOpt("bw", &bw_str));
    p.addopt(new oasys::StringOpt("latency", &latency_str));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    if (bw_str != "" && !parse_bw(bw_str.c_str(), &bw_)) {
        *invalidp = strdup(bw_str.c_str()); // leak!
        return false;
    }

    if (latency_str != "" && !parse_time(latency_str.c_str(), &latency_)) {
        *invalidp = strdup(latency_str.c_str()); // leak!
        return false;
    }

    return true;
}


/**
 * Set the current connectivity state.
 */
void
Connectivity::set_state(const char* n1, const char* n2, const ConnState& s)
{
    oasys::StringBuffer key("%s,%s", n1, n2);
    StateTable::iterator iter = state_.find(key.c_str());
    if (iter != state_.end()) {
        iter->second = s;
    } else {
        state_[key.c_str()] = s;
    }

    log_debug("set state %s,%s: %s bw=%d latency=%d",
              n1, n2, s.open_ ? "up" : "down", s.bw_, s.latency_);
}

/**
 * Accessor to get the current connectivity state.
 */
const ConnState*
Connectivity::lookup(Node* n1, Node* n2)
{
    oasys::StringBuffer buf("%s,%s", n1->name(), n2->name());
    
    return NULL;
}

/**
 * Event handler function.
 */
void
Connectivity::process(SimEvent *e)
{
    if (e->type() != SIM_CONNECTIVITY) {
        PANIC("no Node handler for event %s", e->type_str());
    }

    SimConnectivityEvent* ce = (SimConnectivityEvent*)e;

    set_state(ce->n1_.c_str(), ce->n2_.c_str(), *ce->state_);
    delete ce->state_; // XXX/demmer yuck
}

/**
 * Hook so implementations can handle arbitrary commands.
 */
bool
Connectivity::exec(int argc, const char** argv)
{
    return false;
}
    

} // namespace dtnsim

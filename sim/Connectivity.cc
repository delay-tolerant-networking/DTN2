/*
 *    Copyright 2005-2006 Intel Corporation
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


#include "Connectivity.h"
#include "Node.h"
#include "SimEvent.h"
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>

#include "SimConvergenceLayer.h"
#include "contacts/ContactManager.h"
#include "contacts/Link.h"
#include "naming/EndpointID.h"



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
        log_crit_p("/connectivity", "invalid connectivity module type %s",
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
    (void)argc;
    (void)argv;
    return false;
}
    

} // namespace dtnsim

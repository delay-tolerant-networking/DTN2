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


#include <oasys/util/StringBuffer.h>

#include "bundling/BundleEvent.h"
#include "contacts/Contact.h"
#include "routing/RouteTable.h"
#include "routing/BundleRouter.h"
#include "SimEvent.h"
#include "Simulator.h"
#include "Simdtn2Command.h"
#include "SimConvergenceLayer.h"

namespace dtnsim {

Simdtn2Command Simdtn2Command::instance_;

Simdtn2Command::Simdtn2Command()
    : AutoTclCommand("simdtn2")
{}

void
Simdtn2Command::at_reg()
{
    bind_s("type", &BundleRouter::type_, "static");
}

const char*
Simdtn2Command::help_string()
{
    return "simdtn2 <time> addroute <src> <dest> <contact_id> \n";

}

int
Simdtn2Command::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    if (argc < 6) {
        wrong_num_args(argc, argv, 2, 6, INT_MAX);
        return TCL_ERROR;
    }
    
    long time = atoi(argv[1]) ;
    const char* cmd = argv[2];
    const char* src_str = argv[3];
    const char* dest_str = argv[4];
    const char* next_hop_str = argv[5];
    
    if (strcmp(cmd, "addroute") == 0) {
//      simdtn2 <time> addroute <src> <dest> <contact_id> 
        int cid  = atoi(next_hop_str);
        SimContact* link = Topology::contact(cid);
        Contact* contact = SimConvergenceLayer::simlink2ct(link);
        ASSERT(contact != NULL);
        
        std::string dest_str_full = 
            SimConvergenceLayer::id2node(atoi(dest_str));
        EndpointIDPattern dest(dest_str_full);
        RouteEntry* entry = new RouteEntry(dest, contact,
                                           ForwardingInfo::COPY_ACTION);
        
        log_info("addroute to dest %s using contact id %d",
                 dest_str_full.c_str(), cid);
        // post the event, through the simulator 
        int src = atoi(src_str);
        Event_for_br* e = 
            new Event_for_br(time,Topology::node(src),new RouteAddEvent(entry));
        Simulator::post(e);

    }
    else {
        PANIC("unimplemented command");
    }
    
    return TCL_OK;
}

} // namespace dtnsim

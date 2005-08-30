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

#include <oasys/util/StringBuffer.h>

#include "bundling/Contact.h"
#include "bundling/BundleEvent.h"

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
        RouteEntry* entry = new RouteEntry(dest, contact, FORWARD_COPY);
        
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

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

#include <stdlib.h>

#include "NodeCommand.h"
#include "SimCommand.h"
#include "Simulator.h"
#include "Topology.h"

#include "routing/BundleRouter.h"

// #include "SimConvergenceLayer.h"
// #include "TrAgent.h"

using namespace dtn;

namespace dtnsim {

SimCommand::SimCommand()
    : TclCommand("sim")
{
    bind_d("runtill", &Simulator::runtill_, "Run simulation for this many steps.");
    bind_s("route_type", &BundleRouter::Config.type_, "static",
        "What type of router to use.");
}

int
SimCommand::exec(int argc, const char** argv, Tcl_Interp* tclinterp)
{
    (void)tclinterp;
    if (argc < 3) {
        wrong_num_args(argc, argv, 2, 3, 11);
        return TCL_ERROR;
    }
    
    // pull out the time and subcommand
    char* end;
    double time = strtod(argv[1], &end);
    if (*end != '\0') {
        resultf("time value '%s' invalid", argv[1]);
        return TCL_ERROR;
    }
    const char* cmd = argv[2];
    
    if (strcmp(cmd, "create_node") == 0) {
        // sim <time> node create <name>
        if (argc < 4) {
            wrong_num_args(argc, argv, 2, 4, 4);
            return TCL_ERROR;
        }

        if (time != 0) {
            resultf("all nodes must be created at time 0");
            return TCL_ERROR;
        }

        const char* name = argv[3];

        // make sure no tcl command already exists with the given name
        oasys::TclCommandInterp* interp = oasys::TclCommandInterp::instance();
        if (interp->lookup(name)) {
            resultf("error creating node %s: tcl command already exists",
                    name);
            return TCL_ERROR;
        }
        
        Node* node = Topology::create_node(name);

        NodeCommand* cmd = new NodeCommand(node);
        interp->reg(cmd);
        
        return TCL_OK;
    }
    
//     if (strcmp(cmd, "create_consumer") == 0) {
//         if (argc < 4) {
//             wrong_num_args(argc, argv, 2, 4, 4);
//             return TCL_ERROR;
//         }
//         int id = atoi(argv[3]) ;
//         Topology::create_consumer(id);       
//         log_info("create_consumer %d \n",id);
//     }
    
//     // simulator time create_contact <id> <src> <dst> <bw> <delay> <isup> <up> <down>
//     if (strcmp(cmd, "create_contact") == 0) {
//         if (argc < 11) {
//             wrong_num_args(argc, argv, 2, 11, 11);
//             return TCL_ERROR;
//         }
        
//         int id = atoi(argv[3]) ;
//         int src = atoi(argv[4]) ;
//         int dst = atoi(argv[5]) ;
//         int  bw = atoi(argv[6]) ;
//         int delay = atoi(argv[7]) ;
//         int isup =  atoi(argv[8]) ;
//         int up = atoi(argv[9]) ;
//         int down = atoi(argv[10]) ;
        
//         Topology::create_contact(id,src,dst,bw,delay,isup,up,down);
//         log_info("new contact: (%d->%d), param:[%d,%d] \n",src,dst,bw,delay);



        
        
//     }
    
//     // simulator time tr <size> <batch> <reps> <gap>
//     if (strcmp(cmd, "create_tr") == 0) {
//         if (argc < 9) {
//             wrong_num_args(argc, argv, 2, 9,9);
//             return TCL_ERROR;
//         }
//         int src = atoi(argv[3]) ;
//         int dst = atoi(argv[4]) ;
//         int size = atoi(argv[5]) ;
//         int batch = atoi(argv[6]) ;
//         int reps = atoi(argv[7]) ;
//         int gap = atoi(argv[8]) ;
//         TrAgent* tr = new TrAgent(time,src,dst,size,batch,reps,gap);
//         tr->start();
//         log_info("creating traffic btw (src,dst) (%d,%d)",src,dst);
//     }
    
    
//     if (strcmp(cmd, "cup") == 0) {
//         int id = atoi(argv[3]) ;
//         bool forever = false;
//         if (argc == 5) {
//             if (atoi(argv[4]) != 0) forever = true;
//         }
//         Event_contact_up* e = 
//             new Event_contact_up(time,Topology::contact(id));
//         e->forever_ = forever;
//         Simulator::post(e);
//     }

    
//     if (strcmp(cmd, "cdown") == 0) {
//         int id = atoi(argv[3]) ;
//         bool forever = false;
//         if (argc == 5) {
//             if (atoi(argv[4]) != 0) forever = true;
//         }
        
//         Event_contact_down* e = 
//             new Event_contact_down(time,Topology::contact(id));
//         e->forever_ = forever;
//         Simulator::post(e);
//     }
    
//     if (strcmp(cmd, "print_stats") == 0) {
//         Event_print_stats* e = 
//             new Event_print_stats(time,Simulator::instance());
//         log_info("COM: print_stats at:%3f event:%p",time,e);
//         Simulator::post(e);
//     }

    resultf("sim: unsupported subcommand %s", cmd);
    return TCL_ERROR;
}


} // namespace dtnsim

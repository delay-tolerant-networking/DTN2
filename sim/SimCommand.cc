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


#include <stdlib.h>

#include "NodeCommand.h"
#include "SimCommand.h"
#include "Simulator.h"
#include "Topology.h"

#include "routing/BundleRouter.h"


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
        // sim <time> create_node <name>
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

/* 
    // sim <time> create_contact <id> <src> <dst> <bw> <delay> <isup> <up> <down>
    if (strcmp(cmd, "create_contact") == 0) {
		if (argc < 11) {
               wrong_num_args(argc, argv, 2, 11, 11);
               return TCL_ERROR;
		}

		int id = atoi(argv[3]) ;
		int src = atoi(argv[4]) ;
		int dst = atoi(argv[5]) ;
		int  bw = atoi(argv[6]) ;
		int delay = atoi(argv[7]) ;
		int isup =  atoi(argv[8]) ;
		int up = atoi(argv[9]) ;
		int down = atoi(argv[10]) ;
        
		Topology::create_contact(id,src,dst,bw,delay,isup,up,down);
		log_info("new contact: (%d->%d), param:[%d,%d] \n",src,dst,bw,delay);
		   
		return TCL_OK;
    }

///
    // sim <time> cup <contact_id> <>
	if (strcmp(cmd, "cup") == 0) {
		int id = atoi(argv[3]) ;
		bool forever = false;
		if (argc == 5) {
			if (atoi(argv[4]) != 0) forever = true;
		}
		
		Event_contact_up* e = 
			new Event_contact_up(time,Topology::contact(id));
		
		e->forever_ = forever;
		Simulator::post(e);
		
		return TCL_OK;
    }
/
    // sim <time> cdown <contact_id> <>
	if (strcmp(cmd, "cdown") == 0) {
		int id = atoi(argv[3]) ;
		bool forever = false;
		if (argc == 5) {
			if (atoi(argv[4]) != 0) forever = true;
		}
		Event_contact_down* e = 
			new Event_contact_down(time,Topology::contact(id));
		e->forever_ = forever;
		Simulator::post(e);
		return TCL_OK;
	}
	
	// sim <time> create_tr <src> <dst> <size> <batch> <reps> <gap>
	if (strcmp(cmd, "create_tr") == 0) {
		if (argc < 9) {
			wrong_num_args(argc, argv, 2, 9,9);
			return TCL_ERROR;
		}
		int src = atoi(argv[3]) ;
		int dst = atoi(argv[4]) ;
		int size = atoi(argv[5]) ;
		int batch = atoi(argv[6]) ;
		int reps = atoi(argv[7]) ;
		int gap = atoi(argv[8]) ;
		TrAgent* tr = new TrAgent(time,src,dst,size,batch,reps,gap);
		tr->start();
		log_info("creating traffic btw (src,dst) (%d,%d)",src,dst);
		//return TCL_OK;
	}	
    
     if (strcmp(cmd, "create_consumer") == 0) {
         if (argc < 4) {
             wrong_num_args(argc, argv, 2, 4, 4);
             return TCL_ERROR;
         }
         int id = atoi(argv[3]) ;
         Topology::create_consumer(id);       
         log_info("create_consumer %d \n",id);
     }

	if (strcmp(cmd, "print_stats") == 0) {
           Event_print_stats* e = 
               new Event_print_stats(time,Simulator::instance());
           log_info("COM: print_stats at:%3f event:%p",time,e);
           Simulator::post(e);
       }
*/

    resultf("sim: unsupported subcommand %s", cmd);
    return TCL_ERROR;
}


} // namespace dtnsim

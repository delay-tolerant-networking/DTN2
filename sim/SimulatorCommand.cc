
#include "SimulatorCommand.h"
#include "Simulator.h"
#include "Topology.h"
#include <stdlib.h>
#include "TrAgent.h"

#include "SimConvergenceLayer.h"

SimulatorCommand SimulatorCommand::instance_;

SimulatorCommand::SimulatorCommand() : AutoCommandModule("sim")
{

}


void
SimulatorCommand::at_reg()
{
 bind_i("runtill", &Simulator::runtill_);
 bind_i("nodetype", &Topology::node_type_);

}


const char*
SimulatorCommand::help_string()
{
    return "simulator <time> create_node <id> \n"
        "simulator <time> create_contact <id> <bw> <delay> <uptime> <downtime> <type?> \n"
        "simulator <time> stop"
	"simulator <time> start";
}


int
SimulatorCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    
    if (argc < 3) {
	wrong_num_args(argc, argv, 2, 3, 11);
	return TCL_ERROR;
    }
    
    
    
    long time = atoi(argv[1]) ;
    const char* cmd = argv[2];
    
    
    // simulator time create_node <id>
    if (strcmp(cmd, "create_node") == 0) {
	if (argc < 4) {
	    wrong_num_args(argc, argv, 2, 4, 4);
	    return TCL_ERROR;
	}
	int id = atoi(argv[3]) ;
	Topology::create_node(id);
	log_info("create_node %d \n",id);
    }

    // simulator time create_contact <id> <src> <dst> <bw> <delay> <isup> <up> <down>
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



	
	
    }
    
    // simulator time tr <size> <batch> <reps> <gap>
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
    }
    
    
    if (strcmp(cmd, "cup") == 0) {
	int id = atoi(argv[3]) ;
	bool forever = false;
	if (argc == 5) {
	    if (atoi(argv[4]) != 0) forever = true;
	}
	Event_contact_up* e = new Event_contact_up(time,Topology::contact(id));
	e->forever_ = forever;
	Simulator::add_event(e);
    }

    
    if (strcmp(cmd, "cdown") == 0) {
	int id = atoi(argv[3]) ;
	bool forever = false;
	if (argc == 5) {
	    if (atoi(argv[4]) != 0) forever = true;
	}
	
	Event_contact_down* e = new Event_contact_down(time,Topology::contact(id));
	e->forever_ = forever;
	Simulator::add_event(e);
    }

    return TCL_OK;
}


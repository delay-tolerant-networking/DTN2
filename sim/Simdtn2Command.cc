
#include "bundling/Contact.h"
#include "bundling/BundleEvent.h"

#include "routing/RouteTable.h"
#include "util/StringBuffer.h"
#include "routing/BundleRouter.h"
#include "Event.h"
#include "Simulator.h"
#include "Simdtn2Command.h"
#include "SimConvergenceLayer.h"

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
        BundleTuplePattern dest(dest_str_full);
        RouteEntry* entry = new RouteEntry(dest, contact, FORWARD_COPY);
        
        log_info("addroute to dest %s using contact id %d",
                 dest_str_full.c_str(), cid);
        // post the event, through the simulator 
        int src = atoi(src_str);
        Event_for_br* e = 
            new Event_for_br(time,Topology::node(src),new RouteAddEvent(entry));
        Simulator::add_event(e);

    }
    else {
        PANIC("unimplemented command");
    }
    
    return TCL_OK;
}


#include "bundling/Contact.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "routing/BundleRouter.h"
#include "routing/RouteTable.h"
#include "util/StringBuffer.h"

#include "Event.h"
#include "Simulator.h"
#include "Simdtn2Command.h"
#include "SimConvergenceLayer.h"

Simdtn2Command Simdtn2Command::instance_;

Simdtn2Command::Simdtn2Command() : AutoCommandModule("simdtn2") {}

void
Simdtn2Command::at_reg()
{
    bind_s("type", &BundleRouter::type_, "static");
}

const char*
Simdtn2Command::help_string()
{
    return "simroute add <src> <dest> <contact_id> \n"
	"simroute <src> dump";
}

int
Simdtn2Command::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    if (argc < 3) {
        resultf("need a route subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];
    const char* src_str = argv[2];
//    int src_id = btuple2id(src_str);
	
    if (strcmp(cmd, "add") == 0) {
        // route add <src> <dest> <nexthop>  <type> <args>
	// <nexthop> is a contact id, an integer
	// <src> <dest> are normal bundletuples

        if (argc < 6) {
            wrong_num_args(argc, argv, 2, 6, INT_MAX);
            return TCL_ERROR;
        }
        const char* dest_str = argv[3];
        const char* next_hop_str = argv[4];
        const char* type_str = argv[5];


        BundleTuplePattern dest(dest_str);
        if (!dest.valid()) {
            resultf("invalid destination tuple %s", dest_str);
            return TCL_ERROR;
        }

        /*
        BundleTuple next_hop(next_hop_str);
        if (!next_hop.valid()) {
            resultf("invalid next hop tuple %s", next_hop_str);
            return TCL_ERROR;
        }
	*/

        contact_type_t type = str_to_contact_type(type_str);
        if (type == CONTACT_INVALID) {
            resultf("invalid contact type %s", type_str);
            return TCL_ERROR;
        }

	// XXX/sushant difference from RouteCommand
	int id  = atoi(next_hop_str);
        SimContact* link = Topology::contact(id);
	Contact* contact = SimConvergenceLayer::simlink2ct(link);
	ASSERT(contact != NULL);

        RouteEntry* entry = new RouteEntry(dest, contact, FORWARD_COPY);
        
        // post the event, through the simulator
	BundleTuplePattern src_tuple(src_str);

	int src = SimConvergenceLayer::node2id(src_tuple);

	log_debug("converted ids from %s to %d",src_tuple.admin().c_str(),src);


	Event_for_bundle_router* e = new Event_for_bundle_router(0,Topology::node(src),new RouteAddEvent(entry));
	Simulator::add_event(e);

    }


    else if (strcmp(cmd, "del") == 0) {
        resultf("route delete unimplemented");
        return TCL_ERROR;
    }

    else if (strcmp(cmd, "dump") == 0) {

	resultf("unimplemented route subcommand %s", cmd);
        return TCL_ERROR;
	/*
	 
	StringBuffer buf;
        BundleRouter* router = ((GlueNode *)Topology::node(src_id))->active_router();
        router->route_table()->dump(&buf);
        set_result(buf.c_str());
        return TCL_OK;
	*/

    }
    else {

    }
    
    return TCL_OK;
}

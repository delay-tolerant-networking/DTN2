
#include "RouteCommand.h"
#include "bundling/Contact.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "routing/BundleRouter.h"
#include "routing/RouteTable.h"
#include "util/StringBuffer.h"

RouteCommand RouteCommand::instance_;

RouteCommand::RouteCommand() : AutoCommandModule("route") {}

void
RouteCommand::at_reg()
{
    bind_s("type", &BundleRouter::type_, "static");
}

const char*
RouteCommand::help_string()
{
    return "route add <dest> <nexthop> <type> <args>\n"
        "route del <dest> <nexthop>\n"
        "route dump";
}

int
RouteCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    if (argc < 2) {
        resultf("need a route subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "add") == 0) {
        // route add <dest> <nexthop> <type> <args>
        if (argc < 5) {
            wrong_num_args(argc, argv, 2, 5, INT_MAX);
            return TCL_ERROR;
        }

        const char* dest_str = argv[2];
        const char* next_hop_str = argv[3];
        const char* type_str = argv[4];

        BundleTuplePattern dest(dest_str);
        if (!dest.valid()) {
            resultf("invalid destination tuple %s", dest_str);
            return TCL_ERROR;
        }
        
        BundleTuple next_hop(next_hop_str);
        if (!next_hop.valid()) {
            resultf("invalid next hop tuple %s", next_hop_str);
            return TCL_ERROR;
        }

        contact_type_t type = str_to_contact_type(type_str);
        if (type == CONTACT_INVALID) {
            resultf("invalid contact type %s", type_str);
            return TCL_ERROR;
        }

        // XXX/demmer this should do a lookup in the ContactManager
        // and only create the new contact if it doesn't already exist
        Contact* contact = new Contact(type, next_hop);

        RouteEntry* entry = new RouteEntry(dest, contact, FORWARD_COPY);
        
        // XXX/demmer other parameters?

        // post the event
        BundleForwarder::post(new RouteAddEvent(entry));
    }

    else if (strcmp(cmd, "del") == 0) {
        resultf("route delete unimplemented");
        return TCL_ERROR;
    }

    else if (strcmp(cmd, "dump") == 0) {
        StringBuffer buf;
        BundleRouter* router = BundleForwarder::instance()->active_router();
        router->route_table()->dump(&buf);
        set_result(buf.c_str());
        return TCL_OK;
    }
    else {
        resultf("unimplemented route subcommand %s", cmd);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}


#include "RouteCommand.h"
#include "bundling/Contact.h"
#include "routing/BundleRouter.h"
#include "util/StringBuffer.h"

RouteCommand RouteCommand::instance_;

RouteCommand::RouteCommand() : AutoCommandModule("route") {}

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
        
    // route add <dest> <nexthop> <type> <args>
    if (strcmp(cmd, "add") == 0) {
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

        Contact* contact = new Contact(type, next_hop);

        BundleRouter* router;
        BundleRouterList* routers = BundleRouter::routers();
        BundleRouterList::iterator iter;
        for (iter = routers->begin(); iter != routers->end(); ++iter) {
            router = *iter;

            // XXX/demmer spec for the action_t??
            if (! router->add_route(dest, contact, FORWARD_COPY,
                                    argc - 5, &argv[5]))
            {
                resultf("error adding route %s %s %s",
                        dest_str, next_hop_str, type_str);
                return TCL_ERROR;
            }
        }
    }

    else if (strcmp(cmd, "del") == 0) {
        resultf("route delete unimplemented");
        return TCL_ERROR;
    }

    else if (strcmp(cmd, "dump") == 0) {
        StringBuffer buf;

        BundleRouter* router;
        BundleRouterList* routers = BundleRouter::routers();
        BundleRouterList::iterator iter;
        for (iter = routers->begin(); iter != routers->end(); ++iter) {
            router = *iter;
            router->dump(&buf);
        }

        set_result(buf.c_str());
        return TCL_OK;
    }
    else {
        resultf("unimplemented route subcommand %s", cmd);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}


#include "RouteCommand.h"
#include "routing/RouteTable.h"

RouteCommand RouteCommand::instance_;

RouteCommand::RouteCommand() : AutoCommandModule("route") {}

int
RouteCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    RouteTable* route = RouteTable::instance();

    if (argc < 2) {
        resultf("need a route subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];
        
    // route local_region <region>
    if (strcmp(cmd, "local_region") == 0) {
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }
        route->set_region(argv[2]);
        return TCL_OK;
    }

    // route [add|del] <destregion> <nexthop> <type>
    else if (strcmp(cmd, "add") == 0 ||
             strcmp(cmd, "del") == 0) {
        if (argc != 5) {
            wrong_num_args(argc, argv, 2, 5, 5);
            return TCL_ERROR;
        }

        const char* dst_region = argv[2];
    
        const char* next_hop_str = argv[3];
        const char* type_str = argv[4];
    
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

        if (strcasecmp(argv[1], "add") == 0) {
            if (! route->add_route(dst_region, next_hop, type)) {
                resultf("error adding route %s %s %s", dst_region, next_hop_str,
                        type_str);
                return TCL_ERROR;
            }
        } else if (strcasecmp(argv[1], "del") == 0) {
            if (! route->del_route(dst_region, next_hop, type)) {
                resultf("error removing route %s %s %s", dst_region, next_hop_str,
                        type_str);
                return TCL_ERROR;
            }
        }
        else {
            PANIC("impossible");
        }
    }
    
    else {
        resultf("unknown route subcommand %s", cmd);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}

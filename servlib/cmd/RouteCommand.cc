
#include <oasys/util/StringBuffer.h>

#include "RouteCommand.h"
#include "bundling/Link.h"
#include "bundling/ContactManager.h"

#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleConsumer.h"

#include "routing/BundleRouter.h"
#include "routing/RouteTable.h"

RouteCommand::RouteCommand()
    : TclCommand("route")
{
    bind_s("type", &BundleRouter::type_, "static");
}

const char*
RouteCommand::help_string()
{
    // return "route add <dest> <nexthop> <type> <args>\n"
    return "route add <dest> <link/peer>\n"
        "route del <dest> <link/peer>\n"
        "route local_region <region>\n"
        "route local_tuple <tuple>\n"
        "route dump"
        "         Note: Currently route dump also displays list of all links and peers\n"
        ;
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
        // route add <dest> <link/peer>
        if (argc < 3) {
            wrong_num_args(argc, argv, 2, 3, INT_MAX);
            return TCL_ERROR;
        }

        const char* dest_str = argv[2];

        BundleTuplePattern dest(dest_str);
        if (!dest.valid()) {
            resultf("invalid destination tuple %s", dest_str);
            return TCL_ERROR;
        }
        const char* name = argv[3];
        BundleConsumer* consumer = NULL;
        
        consumer = ContactManager::instance()->find_link(name);
        if (consumer == NULL) {
            BundleTuplePattern peer(name);
            if (!peer.valid()) {
                resultf("invalid peer tuple %s", name);
                return TCL_ERROR;
            }
            consumer = ContactManager::instance()->find_peer(peer);
        }
        
        if (consumer == NULL) {
            resultf("no such link or peer exists, %s", name);
            return TCL_ERROR;
        }
        RouteEntry* entry = new RouteEntry(dest, consumer, FORWARD_COPY);
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
        buf.appendf("local tuple:\n\t%s\n", router->local_tuple_.c_str());
        router->route_table()->dump(&buf);
        ContactManager::instance()->dump(&buf);
        set_result(buf.c_str());
        return TCL_OK;
    }
    
    else if (strcmp(cmd, "local_region") == 0) {
        // route local_region <region>
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }
        
        BundleRouter::local_regions_.push_back(argv[2]);
    }

    else if (strcmp(cmd, "local_tuple") == 0) {
        // route local_tuple <tuple>
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }

        BundleRouter::local_tuple_.assign(argv[2]);
        if (! BundleRouter::local_tuple_.valid()) {
            resultf("invalid tuple '%s'", argv[2]);
            return TCL_ERROR;
        }
    }

    else {
        resultf("unimplemented route subcommand %s", cmd);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}

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

#include <config.h>

#include <oasys/util/StringBuffer.h>
#include <oasys/serialize/XMLSerialize.h>

#include "RouteCommand.h"
#include "CompletionNotifier.h"

#include "contacts/Link.h"
#include "contacts/ContactManager.h"

#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"

#include "routing/BundleRouter.h"
#include "routing/RouteEntry.h"
#include "routing/ExternalRouter.h"

namespace dtn {

RouteCommand::RouteCommand()
    : TclCommand("route")
{
    bind_var(new oasys::StringOpt("type", &BundleRouter::config_.type_, 
                                  "type", "Which routing algorithm to use."));

    bind_var(new oasys::BoolOpt("add_nexthop_routes",
                                &BundleRouter::config_.add_nexthop_routes_,
                                "Whether or not to automatically add routes "
                                "for next hop links"));
    
    bind_var(new oasys::IntOpt("default_priority",
                               &BundleRouter::config_.default_priority_,
                               "priority",
                               "Default priority for new routes "
                               "(initially zero)"));
    
    add_to_help("add <dest> <link/endpoint> [opts]", "add a route");
    add_to_help("del <dest> <link/endpoint>", "delete a route");
    add_to_help("dump", "dump all of the static routes");

#ifdef XERCES_C_ENABLED
    bind_var(new oasys::UInt16Opt("server_port",
                                  &ExternalRouter::server_port,
                                  "port",
                                  "UDP port for IPC with external router(s)"));
    
    bind_var(new oasys::UInt16Opt("hello_interval",
                                  &ExternalRouter::hello_interval,
                                  "interval",
                                  "seconds between hello messages"));
    
    bind_var(new oasys::StringOpt("schema", &ExternalRouter::schema,
                                  "file",
                                  "The external router interface "
                                  "message schema."));

    bind_var(new oasys::BoolOpt("xml_server_validation",
                                &ExternalRouter::server_validation,
                                "Perform xml validation on plug-in "
                                "interface messages (default is true)"));
    
    bind_var(new oasys::BoolOpt("xml_client_validation",
                                &ExternalRouter::client_validation,
                                "Include meta-info in xml messages "
                                "so plug-in routers"
                                "can perform validation (default is false)"));
#endif
}

int
RouteCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    if (argc < 2) {
        resultf("need a route subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];
    
    if (strcmp(cmd, "add") == 0) {
        // route add <dest> <link/endpoint> <args>
        if (argc < 4) {
            wrong_num_args(argc, argv, 2, 4, INT_MAX);
            return TCL_ERROR;
        }

        const char* dest_str = argv[2];

        EndpointIDPattern dest(dest_str);
        if (!dest.valid()) {
            resultf("invalid destination eid %s", dest_str);
            return TCL_ERROR;
        }
        const char* name = argv[3];

        LinkRef link = BundleDaemon::instance()->contactmgr()->find_link(name);

        if (link == NULL) {
            resultf("no such link %s", name);
            return TCL_ERROR;
        }

        RouteEntry* entry = new RouteEntry(dest, link);
        
        // skip over the consumed arguments and parse optional ones.
        // any invalid options are shifted into argv[0]
        argc -= 4;
        argv += 4;
        if (argc != 0 && (entry->parse_options(argc, argv) != argc))
        {
            resultf("invalid argument '%s'", argv[0]);
            return TCL_ERROR;
        }
        
        // post the event -- if the daemon has been started, we wait
        // for the event to be consumed, otherwise we just return
        // immediately. this allows the command to have the
        // appropriate semantics both in the static config file and in
        // an interactive mode
        
        if (BundleDaemon::instance()->started()) {
            BundleDaemon::post_and_wait(new RouteAddEvent(entry),
                                        CompletionNotifier::notifier());
        } else {
            BundleDaemon::post(new RouteAddEvent(entry));
        }

        return TCL_OK;
    }

    else if (strcmp(cmd, "del") == 0) {
        // route del <dest>
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }

        EndpointIDPattern pat(argv[2]);
        if (!pat.valid()) {
            resultf("invalid endpoint id pattern '%s'", argv[2]);
            return TCL_ERROR;
        }

        if (BundleDaemon::instance()->started()) {
            BundleDaemon::post_and_wait(new RouteDelEvent(pat),
                                        CompletionNotifier::notifier());
        } else {
            BundleDaemon::post(new RouteDelEvent(pat));
        }
        
        return TCL_OK;
    }

    else if (strcmp(cmd, "dump") == 0) {
        oasys::StringBuffer buf;
        BundleDaemon::instance()->get_routing_state(&buf);
        set_result(buf.c_str());
        return TCL_OK;
    }

    else if (strcmp(cmd, "local_eid") == 0) {
        if (argc == 2) {
            // route local_eid
            set_result(BundleDaemon::instance()->local_eid().c_str());
            return TCL_OK;
            
        } else if (argc == 3) {
            // route local_eid <eid?>
            BundleDaemon::instance()->set_local_eid(argv[2]);
            if (! BundleDaemon::instance()->local_eid().valid()) {
                resultf("invalid eid '%s'", argv[2]);
                return TCL_ERROR;
            }
            if (! BundleDaemon::instance()->local_eid().known_scheme()) {
                resultf("local eid '%s' has unknown scheme", argv[2]);
                return TCL_ERROR;
            }
        } else {
            wrong_num_args(argc, argv, 2, 2, 3);
            return TCL_ERROR;
        }
    }

    else {
        resultf("unimplemented route subcommand %s", cmd);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}

} // namespace dtn

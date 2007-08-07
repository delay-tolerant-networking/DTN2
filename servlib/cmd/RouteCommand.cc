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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

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
#include "routing/DTLSRConfig.h"

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

    bind_var(new oasys::IntOpt("max_route_to_chain",
                               &BundleRouter::config_.max_route_to_chain_,
                               "length",
                               "Maximum number of route_to links to follow"));

    bind_var(new oasys::StringOpt("dtlsr_area",
                                  &DTLSRConfig::instance()->area_,
                                  "area", "Administrative area for the local node"));

    bind_var(new oasys::EnumOpt("dtlsr_weight_fn",
                                DTLSRConfig::instance()->weight_opts_,
                                (int*)&DTLSRConfig::instance()->weight_fn_,
                                "fn", "Weight function for the graph"));
                                  
    bind_var(new oasys::DoubleOpt("dtlsr_uptime_factor",
                                  &DTLSRConfig::instance()->uptime_factor_,
                                  "pct", "Aging pct for cost of down links"));

    bind_var(new oasys::BoolOpt("dtlsr_keep_down_links",
                                &DTLSRConfig::instance()->keep_down_links_,
                                "Whether or not to retain down links in the graph"));

    bind_var(new oasys::UIntOpt("dtlsr_recompute_delay",
                                &DTLSRConfig::instance()->recompute_delay_,
                                "seconds",
                                "Delay to compute routes after LSA arrives"));

    bind_var(new oasys::UIntOpt("dtlsr_aging_delay",
                                &DTLSRConfig::instance()->aging_interval_,
                                "seconds",
                                "Interval to locally recompute routes"));
    
    bind_var(new oasys::UIntOpt("dtlsr_lsa_interval",
                                &DTLSRConfig::instance()->lsa_interval_,
                                "seconds",
                                "Interval to periodically send LSAs"));
    
    bind_var(new oasys::UIntOpt("dtlsr_lsa_lifetime",
                                &DTLSRConfig::instance()->lsa_lifetime_,
                                "seconds",
                                "Lifetime of LSA bundles"));
    
    add_to_help("add <dest> <link/endpoint> [opts]", "add a route");
    add_to_help("del <dest> <link/endpoint>", "delete a route");
    add_to_help("dump", "dump all of the static routes");

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)
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

        const char* next_hop = argv[3];

        RouteEntry* entry;
        LinkRef link;
        EndpointIDPattern route_to;

        link = BundleDaemon::instance()->contactmgr()->find_link(next_hop);
        if (link != NULL) {
            entry = new RouteEntry(dest, link);
        } else if (route_to.assign(next_hop)) {
            entry = new RouteEntry(dest, route_to);
        } else {
            resultf("next hop %s is not a valid link or endpoint id",
                    next_hop);
            return TCL_ERROR;
        }

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

    else if (strcmp(cmd, "dump_tcl") == 0) {
        // XXX/demmer this could be done better
        oasys::StringBuffer buf;
        BundleDaemon::instance()->router()->tcl_dump_state(&buf);
        set_result(buf.c_str());
        return TCL_OK;
    }

    else if (strcmp(cmd, "recompute_routes") == 0) {
        oasys::Time t = oasys::Time::now();
        BundleDaemon::instance()->router()->recompute_routes();
        resultf("%u", t.elapsed_ms());
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

/*
 *    Copyright 2005-2006 Intel Corporation
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

#include "Node.h"
#include "NodeCommand.h"
#include "SimConvergenceLayer.h"
#include "SimRegistration.h"
#include "Simulator.h"
#include "Topology.h"
#include "TrAgent.h"
#include "bundling/Bundle.h"
#include "contacts/ContactManager.h"
#include "contacts/Link.h"
#include "naming/EndpointID.h"
#include "routing/BundleRouter.h"
#include "routing/RouteTable.h"
#include "reg/RegistrationTable.h"

using namespace dtn;

namespace dtnsim {

NodeCommand::NodeCommand(Node* node)
    : TclCommand(node->name()), node_(node)
{
}

int
NodeCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    if (argc < 2) {
        wrong_num_args(argc, argv, 2, 2, INT_MAX);
    add_to_help("route", "XXX");
    add_to_help("link", "XXX");
    add_to_help("tragent", "XXX");
    add_to_help("registration", "XXX");
        return TCL_ERROR;
    }

    // for all commands and their side-effects, install the node as
    // the "singleton" BundleDaemon instance, and store the command
    // time in the node so any posted events happen in the future
    node_->set_active();

    const char* cmd = argv[1];
    const char* subcmd = NULL;
    if (argc >= 3) {
        subcmd = argv[2];
    }

    if (strcmp(cmd, "route") == 0)
    {
        return route_cmd_.exec(argc - 1, argv + 1, interp);
    }
//         if (strcmp(subcmd, "local_eid") == 0) {
//             if (time != 0) {
//                 resultf("node %s %s must be run at time 0", cmd, subcmd);
//                 return TCL_ERROR;
//             }
        
//             if (argc == 4) {
//                 // <node> 0 route local_eid
//                 set_result(node_->local_eid().c_str());
//                 return TCL_OK;
                
//             } else if (argc == 5) {
//                 // <node> 0 route local_eid <eid>
//                 node_->set_local_eid(argv[4]);

//                 if (! node_->local_eid().valid()) {
//                     resultf("invalid eid '%s'", argv[4]);
//                     return TCL_ERROR;
//                 }
//                 return TCL_OK;
//             } else {
//                 wrong_num_args(argc, argv, 4, 4, 5);
//                 return TCL_ERROR;
//             }
//         }
//         else if (strcmp(subcmd, "add") == 0)
//         {
//             // <node> X route add <dest> <link/node>
//             if (argc != 6) {
//                 wrong_num_args(argc, argv, 2, 6, 6);
//                 return TCL_ERROR;
//             }

//             const char* dest_str = argv[4];
//             const char* nexthop = argv[5];

//             log_debug("adding route to %s through %s", dest_str, nexthop);
            
//             EndpointIDPattern dest(dest_str);
//             if (!dest.valid()) {
//                 resultf("invalid destination eid %s", dest_str);
//                 return TCL_ERROR;
//             }

//             Simulator::post(new SimAddRouteEvent(time, node_, dest, nexthop));
            
//             return TCL_OK;
//         }
        
//         resultf("node route: unsupported subcommand %s", subcmd);
//         return TCL_ERROR;
//     }
	
    else if (strcmp(cmd, "link") == 0)
    {
        return link_cmd_.exec(argc - 1, argv + 1, interp);
    }
    else if (strcmp(cmd, "registration") == 0)
    {
        if (strcmp(subcmd, "add") == 0) {
            // <node> registration add <eid>
            const char* eid_str = argv[3];
            EndpointIDPattern eid(eid_str);

            if (!eid.valid()) {
                resultf("error in node registration add %s: "
                        "invalid demux eid", eid_str);
                return TCL_ERROR;
            }

            Registration* r = new SimRegistration(node_, eid);
            RegistrationAddedEvent* e =
                new RegistrationAddedEvent(r, EVENTSRC_ADMIN);

            node_->post_event(e);
            
            return TCL_OK;
        }        
        resultf("node registration: unsupported subcommand %s", subcmd);
        return TCL_ERROR;
    }

    else if (strcmp(cmd, "tragent") == 0)
    {
        // <node> tragent <src> <dst> <args>
        if (argc < 5) {
            wrong_num_args(argc, argv, 3, 5, INT_MAX);
            return TCL_ERROR;
        }
        
        const char* src = argv[2];
        const char* dst = argv[3];

        // see if src/dest are node names, in which case we use its
        // local eid as the source address
        EndpointID src_eid;
        Node* src_node = Topology::find_node(src);
        if (src_node) {
            src_eid.assign(src_node->local_eid());
        } else {
            src_eid.assign(src);
            if (!src_eid.valid()) {
                resultf("node tragent: invalid src eid %s", src);
                return TCL_ERROR;
            }
        }
        
        EndpointID dst_eid;
        Node* dst_node = Topology::find_node(dst);
        if (dst_node) {
            dst_eid.assign(dst_node->local_eid());
        } else {
            dst_eid.assign(dst);
            if (!dst_eid.valid()) {
                resultf("node tragent: invalid dst eid %s", dst);
                return TCL_ERROR;
            }
        }
        
        TrAgent* a = TrAgent::init(src_eid, dst_eid,
                                   argc - 4, argv + 4);
        if (!a) {
            resultf("error in tragent config");
            return TCL_ERROR;
        }
        
        return TCL_OK;

    } else if (!strcmp(cmd, "stats")) {
        oasys::StringBuffer buf("Bundle Statistics: ");
        node_->get_bundle_stats(&buf);
        set_result(buf.c_str());
        return TCL_OK;
    }
    else
    {
        resultf("node: unsupported subcommand %s", cmd);
        return TCL_ERROR;
    }
}

} // namespace dtnsim

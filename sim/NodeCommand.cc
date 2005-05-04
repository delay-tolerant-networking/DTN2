/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "bundling/BundleMapping.h"
#include "bundling/BundleTuple.h"
#include "bundling/ContactManager.h"
#include "bundling/Link.h"
#include "routing/BundleRouter.h"
#include "routing/RouteTable.h"


using namespace dtn;

namespace dtnsim {

NodeCommand::NodeCommand(Node* node)
    : TclCommand(node->name()), node_(node)
{
}


const char*
NodeCommand::help_string()
{
    return "(see documentation)";
}

int
NodeCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    if (argc < 3) {
        wrong_num_args(argc, argv, 2, 4, INT_MAX);
        return TCL_ERROR;
    }

    // pull out the time and subcommand
    char* end;
    double time = strtod(argv[1], &end);
    if (*end != '\0') {
        resultf("time value '%s' invalid", argv[1]);
        return TCL_ERROR;
    }

    // for all commands and their side-effects, install the node as
    // the "singleton" BundleDaemon instance, and store the command
    // time in the node so any posted events happen in the future
    node_->set_active();

    const char* cmd = argv[2];
    const char* subcmd = NULL;
    if (argc >= 4) {
        subcmd = argv[3];
    }

    if (strcmp(cmd, "route") == 0)
    {
        if (strcmp(subcmd, "local_tuple") == 0) {
            if (time != 0) {
                resultf("node %s %s must be run at time 0", cmd, subcmd);
                return TCL_ERROR;
            }
        
            if (argc == 4) {
                // <node> 0 route local_tuple
                set_result(node_->router()->local_tuple().c_str());
                return TCL_OK;
                
            } else if (argc == 5) {
                // <node> 0 route local_tuple <tuple>
                node_->router()->set_local_tuple(argv[4]);

                if (! node_->router()->local_tuple().valid()) {
                    resultf("invalid tuple '%s'", argv[4]);
                    return TCL_ERROR;
                }
                return TCL_OK;
            } else {
                wrong_num_args(argc, argv, 4, 4, 5);
                return TCL_ERROR;
            }
        }
        else if (strcmp(subcmd, "add") == 0)
        {
            // <node> X route add <dest> <link/node>
            if (argc != 6) {
                wrong_num_args(argc, argv, 2, 6, 6);
                return TCL_ERROR;
            }

            const char* dest_str = argv[4];
            const char* nexthop = argv[5];

            log_debug("adding route to %s through %s", dest_str, nexthop);
            
            BundleTuplePattern dest(dest_str);
            if (!dest.valid()) {
                resultf("invalid destination tuple %s", dest_str);
                return TCL_ERROR;
            }

            Simulator::post(new SimAddRouteEvent(time, node_, dest, nexthop));
            
            return TCL_OK;
        }
        
        resultf("node route: unsupported subcommand %s", subcmd);
        return TCL_ERROR;
    }
    else if (strcmp(cmd, "link") == 0)
    {
        if (strcmp(subcmd, "add") == 0) {
            // <node1> X link add <name> <peer> <type> <args>
            if (argc < 6) {
                wrong_num_args(argc, argv, 2, 7, INT_MAX);
                return TCL_ERROR;
            }

            const char* name = argv[4];
            const char* nexthop_name = argv[5];
            const char* type_str = argv[6];
            
            Node* nexthop = Topology::find_node(nexthop_name);

            if (!nexthop) {
                resultf("invalid next hop node %s", nexthop_name);
                return TCL_ERROR;
            }
                
            Link::link_type_t type = Link::str_to_link_type(type_str);
            if (type == Link::LINK_INVALID) {
                resultf("invalid link type %s", type_str);
                return TCL_ERROR;
            }

            SimConvergenceLayer* simcl = SimConvergenceLayer::instance();
            Link* link = Link::create_link(name, type, simcl, nexthop->name(),
                                           argc - 7, &argv[7]);
            if (!link)
                return TCL_ERROR;
            
            Simulator::post(new SimAddLinkEvent(time, node_, link));

            return TCL_OK;
        }

        resultf("node link: unsupported subcommand %s", subcmd);
        return TCL_ERROR;
    }
    else if (strcmp(cmd, "registration") == 0)
    {
        if (strcmp(subcmd, "add") == 0) {
            // <node> X registration add <demux_tuple>
            const char* demux_str = argv[4];
            BundleTuplePattern demux_tuple(demux_str);

            if (!demux_tuple.valid()) {
                resultf("error in node registration add %s: "
                        "invalid demux tuple", demux_str);
                return TCL_ERROR;
            }

            Registration* r = new SimRegistration(node_, demux_tuple);
            RegistrationAddedEvent* e = new RegistrationAddedEvent(r);
            Simulator::post(new SimRouterEvent(time, node_, e));
            
            return TCL_OK;
        }        
        resultf("node registration: unsupported subcommand %s", subcmd);
        return TCL_ERROR;
    }

    else if (strcmp(cmd, "tragent") == 0)
    {
        if (argc < 5) {
            wrong_num_args(argc, argv, 3, 5, INT_MAX);
            return TCL_ERROR;
        }
        
        const char* src = argv[3];
        const char* dst = argv[4];

        // see if src/dest are node names, in which case we use its
        // local tuple as the source address
        BundleTuple src_tuple;
        Node* src_node = Topology::find_node(src);
        if (src_node) {
            src_tuple.assign(src_node->router()->local_tuple());
        } else {
            src_tuple.assign(src);
            if (!src_tuple.valid()) {
                resultf("node tragent: invalid src tuple %s", src);
                return TCL_ERROR;
            }
        }
        
        BundleTuple dst_tuple;
        Node* dst_node = Topology::find_node(dst);
        if (dst_node) {
            dst_tuple.assign(dst_node->router()->local_tuple());
        } else {
            dst_tuple.assign(dst);
            if (!dst_tuple.valid()) {
                resultf("node tragent: invalid dst tuple %s", dst);
                return TCL_ERROR;
            }
        }
        
        TrAgent* a = TrAgent::init(node_, time, src_tuple, dst_tuple,
                                   argc - 5, argv + 5);
        if (!a) {
            resultf("error in tragent config");
            return TCL_ERROR;
        }
        
        return TCL_OK;
    }

    else
    {
        resultf("node: unsupported subcommand %s", cmd);
        return TCL_ERROR;
    }
}

} // namespace dtnsim

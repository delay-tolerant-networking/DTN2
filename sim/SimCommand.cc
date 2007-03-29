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

#include <stdlib.h>

#include "NodeCommand.h"
#include "SimCommand.h"
#include "Simulator.h"
#include "Topology.h"

#include "routing/BundleRouter.h"


using namespace dtn;

namespace dtnsim {

SimCommand::SimCommand()
    : TclCommand("sim")
{
    bind_var(new oasys::DoubleOpt("runtill", &Simulator::runtill_,
                                  "steps", "Run simulation for this many steps"));
    bind_var(new oasys::StringOpt("route_type", &BundleRouter::config_.type_,
                                  "type", "What type of router to use"));
}

int
SimCommand::exec(int argc, const char** argv, Tcl_Interp* tclinterp)
{
    (void)tclinterp;
    if (argc < 2) {
        wrong_num_args(argc, argv, 2, 2, INT_MAX);
        return TCL_ERROR;
    }

    const char* cmd = argv[1];
    if (strcmp(cmd, "create_node") == 0) {
        // sim create_node <name>
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }

        const char* name = argv[2];

        // make sure no tcl command already exists with the given name
        oasys::TclCommandInterp* interp = oasys::TclCommandInterp::instance();
        if (interp->lookup(name)) {
            resultf("error creating node %s: tcl command already exists",
                    name);
            return TCL_ERROR;
        }
        
        Node* node = Topology::create_node(name);

        NodeCommand* cmd = new NodeCommand(node);
        interp->reg(cmd);
        
        return TCL_OK;

    } else if (strcmp(cmd, "at") == 0) {
        // sim at <time> <cmd...>
        if (argc < 4) {
            wrong_num_args(argc, argv, 2, 4, INT_MAX);
            return TCL_ERROR;
        }

        char* end;
        double time = strtod(argv[2], &end);
        if (*end != '\0') {
            resultf("time value '%s' invalid", argv[1]);
            return TCL_ERROR;
        }

        std::string cmd = "";
        for (int i = 3; i < argc; ++i) {
            cmd += argv[i];
            cmd += " ";
        }
        Simulator::post(new SimAtEvent(time, Simulator::instance(), cmd));
        return TCL_OK;
        
    } else if (strcmp(cmd, "run") == 0) {
        Simulator::instance()->run();
        return TCL_OK;

    } else if (strcmp(cmd, "pause") == 0) {
        Simulator::instance()->pause();
        return TCL_OK;
    }

    resultf("sim: unsupported subcommand %s", cmd);
    return TCL_ERROR;
}


} // namespace dtnsim

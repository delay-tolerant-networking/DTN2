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

#include "Connectivity.h"
#include "ConnCommand.h"
#include "Simulator.h"
#include "Topology.h"

namespace dtnsim {

ConnCommand::ConnCommand()
    : TclCommand("conn")
{
    bind_s("type", &Connectivity::type_, "Connectivity type.");
    
    add_to_help("up", "Take connection up XXX");
    add_to_help("down", "Take connection down XXX");
}

int
ConnCommand::exec(int argc, const char** argv, Tcl_Interp* tclinterp)
{
    if (argc < 3) {
        wrong_num_args(argc, argv, 2, 4, INT_MAX);
        return TCL_ERROR;
    }
    
    // pull out the time
    char* end;
    double time = strtod(argv[1], &end);
    if (*end != '\0') {
        resultf("time value '%s' invalid", argv[1]);
        return TCL_ERROR;
    }

    const char* cmd = argv[2];

    Connectivity* conn = Connectivity::instance();

    if (!strcmp(cmd, "up") || !strcmp(cmd, "down")) {
        // conn <time> <up|down> <n1> <n2> <args>
        if (argc < 5) {
            wrong_num_args(argc, argv, 2, 5, INT_MAX);
            return TCL_ERROR;
        }

        const char* n1_name = argv[3];
        const char* n2_name = argv[4];

        if (strcmp(n1_name, "*") != 0 &&
            Topology::find_node(n1_name) == NULL)
        {
            resultf("invalid node or wildcard '%s'", n1_name);
            return TCL_ERROR;
        }
        
        if (strcmp(n2_name, "*") != 0 &&
            Topology::find_node(n2_name) == NULL)
        {
            resultf("invalid node or wildcard '%s'", n2_name);
            return TCL_ERROR;
        }
        
        ConnState* s = new ConnState();
        const char* invalid;
        if (! s->parse_options(argc - 5, argv + 5, &invalid)) {
            resultf("invalid option '%s'", invalid);
            delete s;
            return TCL_ERROR;
        }
        
        s->open_ = !strcmp(cmd, "up");

        Simulator::post(
            new SimConnectivityEvent(time, conn, n1_name, n2_name, s));

        return TCL_OK;

    } else {
        // dispatch to the connectivity module itself
        if (! conn->exec(argc - 3, argv + 3)) {
            resultf("conn: error handling command");
            return TCL_ERROR;
        }


        return TCL_OK;
    }
    
    resultf("conn: unsupported subcommand %s", cmd);
    return TCL_ERROR;
}


} // namespace dtnsim

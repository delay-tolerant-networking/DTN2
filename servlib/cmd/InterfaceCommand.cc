/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
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

#include "InterfaceCommand.h"
#include "bundling/InterfaceTable.h"
#include "conv_layers/ConvergenceLayer.h"

InterfaceCommand::InterfaceCommand()
    : TclCommand("interface") {}

const char*
InterfaceCommand::help_string()
{
    return("interface add <conv_layer> <tuple> [<args>?]\n"
           "interface del <conv_layer> <tuple>");
}

int
InterfaceCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    if (argc < 4) {
        wrong_num_args(argc, argv, 1, 4, INT_MAX);
        return TCL_ERROR;
    }
    
    // interface add <conv_layer> <tuple>
    // interface del <conv_layer> <tuple>

    const char* proto = argv[2];
    const char* tuplestr = argv[3];

    ConvergenceLayer* cl = ConvergenceLayer::find_clayer(proto);
    if (!cl) {
        resultf("can't find convergence layer for %s", proto);
        return TCL_ERROR;
    }

    BundleTuple tuple(tuplestr);
    if (!tuple.valid()) {
        resultf("invalid interface tuple '%s'", tuplestr);
        return TCL_ERROR;
    }

    if (strcasecmp(argv[1], "add") == 0) {
        if (! InterfaceTable::instance()->add(tuple, cl, proto,
                                              argc - 3, argv + 3)) {
            resultf("error adding interface %s", argv[1]);
            return TCL_ERROR;
        }
    } else if (strcasecmp(argv[1], "del") == 0) {
        if (argc != 4) {
            wrong_num_args(argc, argv, 2, 4, 4);
            return TCL_ERROR;
        }

        if (! InterfaceTable::instance()->del(tuple, cl, proto)) {
            resultf("error removing interface %s", argv[2]);
            return TCL_ERROR;
        }

    } else {
        resultf("invalid interface subcommand %s", argv[1]);
        return TCL_ERROR;
    }

    return TCL_OK;
}

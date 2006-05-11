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

#include <oasys/util/StringBuffer.h>

#include "RouteCommand.h"
#include "CompletionNotifier.h"

#include "contacts/Link.h"
#include "contacts/ContactManager.h"

#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"

#include "routing/BundleRouter.h"
#include "routing/RouteEntry.h"

namespace dtn {

RouteCommand::RouteCommand()
    : TclCommand("route")
{
    bind_s("type", &BundleRouter::Config.type_, "static",
        "Which routing algorithm to use.");
    add_to_help("add <dest> <link/endpoint> [opts]", "add a route");
    add_to_help("del <dest> <link/endpoint>", "delete a route");
    add_to_help("dump", "dump all of the static routes");
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
        Link* link = NULL;
        
        link = BundleDaemon::instance()->contactmgr()->find_link(name);

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

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
        oasys::StringBuffer buf;
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

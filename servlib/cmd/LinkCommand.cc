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

#include "LinkCommand.h"
#include "bundling/AddressFamily.h"
#include "bundling/Link.h"
#include "bundling/ContactManager.h"
#include <oasys/util/StringBuffer.h>

namespace dtn {

LinkCommand::LinkCommand()
    : TclCommand("link")
{
}

const char*
LinkCommand::help_string()
{
    return ""
        "link add <name> <nexthop> <type> <convergence_layer> <args>\n"
        "      Note: <nexthop> is a bundle admin identifier, <name> is any string \n"
        ;
}

int
LinkCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    if (argc < 2) {
        resultf("need a link subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "add") == 0) {
        // link add <name> <nexthop> <type> <clayer> <args>
        if (argc < 5) {
            wrong_num_args(argc, argv, 2, 3, INT_MAX);
            return TCL_ERROR;
        }
        
        const char* name = argv[2];
        const char* nexthop = argv[3];
        const char* type_str = argv[4];
        const char* cl = argv[5];

        bool valid;
        AddressFamily* af = AddressFamilyTable::instance()->lookup(nexthop, &valid);
        
        if (!af || !valid) {
            resultf("invalid next hop admin string '%s'", nexthop);
            return TCL_ERROR;
        }
        
        
        Link::link_type_t type = Link::str_to_link_type(type_str);
        if (type == Link::LINK_INVALID) {
            resultf("invalid link type %s", type_str);
            return TCL_ERROR;
        }

        Link* link = ContactManager::instance()->find_link(name);
        if (link != NULL) {
            resultf("link name %s already exists, use different name", name);
            return TCL_ERROR;
        }

        // XXX/Sushant pass other parameters?
        link = Link::create_link(name, type, cl, nexthop);
        
    }
    else {
        resultf("unimplemented link subcommand %s", cmd);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}

} // namespace dtn

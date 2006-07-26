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


#include <oasys/thread/Lock.h>
#include <oasys/util/StringBuffer.h>

#include "LinkCommand.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "contacts/Link.h"
#include "contacts/ContactManager.h"
#include "conv_layers/ConvergenceLayer.h"
#include "naming/Scheme.h"

namespace dtn {

LinkCommand::LinkCommand()
    : TclCommand("link")
{
    add_to_help("add <name> <next hop> <type> <conv layer> <args>", "add links");
    add_to_help("open <name>", "open the link");
    add_to_help("close <name>", "close the link");
    add_to_help("set_available <name> <true | false>", 
                "hacky way to make the link available");
    add_to_help("state <name>", "return the state of a link");
    add_to_help("stats <name>", "dump link statistics");
    add_to_help("dump <name?>", "print the list of existing links or "
                "detailed info about a single link");
    add_to_help("reconfigure <name> <opt=val> <opt2=val2>...",
                "configure link options after initialization "
                "(not all options support this feature)");
    add_to_help("set_cl_defaults <cl> <opt=val> <opt2=val2>...",
                "configure convergence layer specific default options");
}

int
LinkCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    if (argc < 2) {
        resultf("need a link subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "add") == 0) {
        // link add <name> <nexthop> <type> <clayer> <args>
        if (argc < 6) {
            wrong_num_args(argc, argv, 2, 6, INT_MAX);
            return TCL_ERROR;
        }
        
        const char* name = argv[2];
        const char* nexthop = argv[3];
        const char* type_str = argv[4];
        const char* cl_str = argv[5];

        // validate the link type, make sure there's no link of the
        // same name, and validate the convergence layer
        Link::link_type_t type = Link::str_to_link_type(type_str);
        if (type == Link::LINK_INVALID) {
            resultf("invalid link type %s", type_str);
            return TCL_ERROR;
        }

        ConvergenceLayer* cl = ConvergenceLayer::find_clayer(cl_str);
        if (!cl) {
            resultf("invalid convergence layer %s", cl_str);
            return TCL_ERROR;
        }

        Link* link = BundleDaemon::instance()->contactmgr()->find_link(name);
        if (link != NULL) {
            resultf("link name %s already exists, use different name", name);
            return TCL_ERROR;
        }

        // Create the link, parsing the cl-specific next hop string
        // and other arguments
        const char* invalid_arg = "(unknown)";
        link = Link::create_link(name, type, cl, nexthop, argc - 6, &argv[6],
                                 &invalid_arg);
        if (!link) {
            resultf("invalid link option: %s", invalid_arg);
            return TCL_ERROR;
        }

        // Add the link to contact manager's table, which posts a
        // LinkCreatedEvent to the daemon
        BundleDaemon::instance()->contactmgr()->add_link(link);
        return TCL_OK;

    } else if (strcmp(cmd, "reconfigure") == 0) {
        // link set_option <name> <opt=val> <opt2=val2>...
        if (argc < 4) {
            wrong_num_args(argc, argv, 2, 4, INT_MAX);
            return TCL_ERROR;
        }

        const char* name = argv[2];
        
        Link* link = BundleDaemon::instance()->contactmgr()->find_link(name);
        if (link == NULL) {
            resultf("link %s doesn't exist", name);
            return TCL_ERROR;
        } 

        argc -= 3;
        argv += 3;

        const char* invalid;
        int count = link->parse_args(argc, argv, &invalid);
        if (count == -1) {
            resultf("invalid link option: %s", invalid);
            return TCL_ERROR;
        }
        argc -= count;
        
        if (! link->clayer()->reconfigure_link(link, argc, argv)) {
            return TCL_ERROR;
        }
        
        return TCL_OK;
        
    } else if (strcmp(cmd, "open") == 0) {
        // link open <name>
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }

        const char* name = argv[2];
        
        Link* link = BundleDaemon::instance()->contactmgr()->find_link(name);
        if (link == NULL) {
            resultf("link %s doesn't exist", name);
            return TCL_ERROR;
        }

        if (link->isopen()) {
            resultf("link %s already open", name);
            return TCL_OK;
        }

        // XXX/TODO should change all these to post_and_wait
        BundleDaemon::post(new LinkStateChangeRequest(link, Link::OPEN,
                                                      ContactEvent::USER));
        
    } else if (strcmp(cmd, "close") == 0) {
        // link close <name>
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }

        const char* name = argv[2];
        
        Link* link = BundleDaemon::instance()->contactmgr()->find_link(name);
        if (link == NULL) {
            resultf("link %s doesn't exist", name);
            return TCL_ERROR;
        }

        if (! link->isopen() && ! link->isopening()) {
            resultf("link %s already closed", name);
            return TCL_OK;
        }

        BundleDaemon::instance()->post(
            new LinkStateChangeRequest(link, Link::CLOSED,
                                       ContactEvent::USER));

        return TCL_OK;

    } else if (strcmp(cmd, "set_available") == 0) {
        // link set_available <name> <true|false>
        if (argc != 4) {
            wrong_num_args(argc, argv, 2, 4, 4);
            return TCL_ERROR;
        }

        const char* name = argv[2];

        Link* link = BundleDaemon::instance()->contactmgr()->find_link(name);
        if (link == NULL) {
            resultf("link %s doesn't exist", name);
            return TCL_ERROR;
        }

        int len = strlen(argv[3]);
        bool set_available;

        if (strncmp(argv[3], "1", len) == 0) {
            set_available = true;
        } else if (strncmp(argv[3], "0", len) == 0) {
            set_available = false;
        } else if (strncasecmp(argv[3], "true", len) == 0) {
            set_available = true;
        } else if (strncasecmp(argv[3], "false", len) == 0) {
            set_available = false;
        } else if (strncasecmp(argv[3], "on", len) == 0) {
            set_available = true;
        } else if (strncasecmp(argv[3], "off", len) == 0) {
            set_available = false;
        } else {
            resultf("error converting argument %s to boolean value", argv[3]);
            return TCL_ERROR;
        }

        if (set_available) {
            if (link->state() != Link::UNAVAILABLE) {
                resultf("link %s already in state %s",
                        name, Link::state_to_str(link->state()));
                return TCL_OK;
            }

            BundleDaemon::post(
                new LinkStateChangeRequest(link, Link::AVAILABLE,
                                           ContactEvent::USER));
            
            return TCL_OK;

        } else { // ! set_available
            if (link->state() != Link::AVAILABLE) {
                resultf("link %s can't be set unavailable in state %s",
                        name, Link::state_to_str(link->state()));
                return TCL_OK;
            }
            
            BundleDaemon::post(
                new LinkStateChangeRequest(link, Link::UNAVAILABLE,
                                           ContactEvent::USER));
    
            return TCL_OK;
        }
    }
    else if ((strcmp(cmd, "state") == 0) ||
             (strcmp(cmd, "stats") == 0))
    {
        // link state <name>
        // link stats <name>
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }

        const char* name = argv[2];

        Link* link = BundleDaemon::instance()->contactmgr()->find_link(name);
        if (link == NULL) {
            resultf("link %s doesn't exist", name);
            return TCL_ERROR;
        }

        if (strcmp(cmd, "state") == 0) {
            resultf("%s", Link::state_to_str(link->state()));
        } else {
            oasys::StringBuffer buf;
            link->dump_stats(&buf);
            set_result(buf.c_str());
        }
        return TCL_OK;
    }
    else if (strcmp(cmd, "dump") == 0) 
    {
        // link dump <name?>
        if (argc == 2) {
            ContactManager* cm = BundleDaemon::instance()->contactmgr();
            oasys::ScopeLock l(cm->lock(), "LinkCommand::exec");
            
            const LinkSet* links = cm->links();
            for (LinkSet::const_iterator i = links->begin();
                 i != links->end(); ++i)
            {
                append_resultf("*%p\n", *i);
            }
        } else if (argc == 3) {
            const char* name = argv[2];
            
            Link* link = BundleDaemon::instance()->contactmgr()->find_link(name);
            if (link == NULL) {
                resultf("link %s doesn't exist", name);
                return TCL_ERROR;
            }

            oasys::StringBuffer buf;
            link->dump(&buf);
            set_result(buf.c_str());
            return TCL_OK;
        } else {
            wrong_num_args(argc, argv, 2, 2, 3);
            return TCL_ERROR;
        }
    }
    else if (strcmp(cmd, "set_cl_defaults") == 0)
    {
        // link set_cl_defaults <cl> <opt=val> <opt2=val2> ...
        if (argc < 4) {
            wrong_num_args(argc, argv, 2, 4, INT_MAX);
            return TCL_ERROR;
        }

        ConvergenceLayer* cl = ConvergenceLayer::find_clayer(argv[2]);
        if (cl == NULL) {
            resultf("unknown convergence layer %s", argv[2]);
            return TCL_ERROR;
        }

        const char* invalid;
        if (!cl->set_link_defaults(argc - 3, &argv[3], &invalid)) {
            resultf("invalid link option: %s", invalid);
            return TCL_ERROR;
        }
        
        return TCL_OK;
    }
    else
    {
        resultf("unimplemented link subcommand %s", cmd);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}

} // namespace dtn

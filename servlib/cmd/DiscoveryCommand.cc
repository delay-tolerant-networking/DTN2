/*
 *    Copyright 2006 Baylor University
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

#include "DiscoveryCommand.h"
#include "discovery/Discovery.h"
#include "discovery/DiscoveryTable.h"
#include "conv_layers/ConvergenceLayer.h"
#include <oasys/util/StringBuffer.h>

namespace dtn {

DiscoveryCommand::DiscoveryCommand()
    : TclCommand("discovery")
{
    add_to_help("add <discovery_name> <address_family> <port=N> "
                "[<addr=A.B.C.D> <local_addr=A.B.C.D> <multicast_ttl=N> "
                "<channel=N> <unicast=true|false>]",
                "add discovery agent (address family can be ip or bt "
                "[Bluetooth])");
    add_to_help("del <discovery_name>","remove discovery agent"); 
    // discovery add_cl <name> <discovery name> <cl type> [<args>]
    add_to_help("announce <cl_name> <discovery_name> <cl_type> "
                "<interval=N> [<cl_addr=A.B.C.D> <cl_port=N>]",
                "announce the address of a local interface (convergence "
                "layer)");
    add_to_help("remove <name>","remove announcement for local interface");
    add_to_help("list","list all discovery agents and announcements");
}

int
DiscoveryCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    if (strncasecmp("list",argv[1],4) == 0)
    {
        if (argc > 2)
        {
            wrong_num_args(argc, argv, 1, 2, 2);
        }

        oasys::StringBuffer buf;
        DiscoveryTable::instance()->dump(&buf);
        set_result(buf.c_str());

        return TCL_OK;
    }
    else
    // discovery add <name> <address family> <port>
    // [<local_addr> <addr> <multicast_ttl>  <channel>]
    if (strncasecmp("add",argv[1],3) == 0)
    {
        if (argc < 4)
        {
            wrong_num_args(argc, argv, 2, 4, INT_MAX);
            return TCL_ERROR;
        }

        const char* name = argv[2];
        const char* afname = argv[3];

        if (! DiscoveryTable::instance()->add(name,afname,argc - 4,argv + 4))
        {
            resultf("error adding agent %s", name);
            return TCL_ERROR;
        }
        return TCL_OK;
    }
    else
    // discovery del <name>
    if (strncasecmp("del",argv[1],3) == 0)
    {
        if (argc != 3)
        {
            wrong_num_args(argc,argv,2,3,3);
            return TCL_ERROR;
        }

        const char* name = argv[2];
        
        if (! DiscoveryTable::instance()->del(name))
        {
            resultf("error removing agent %s", name);
            return TCL_ERROR;
        }

        return TCL_OK;
    }
    // discovery announce <name> <discovery name> <cl type> <interval> 
    //                    [<cl_addr> <cl_port>]
    else
    if (strncasecmp("announce",argv[1],8) == 0)
    {
        if (argc < 6)
        {
            wrong_num_args(argc,argv,2,6,INT_MAX);
            return TCL_ERROR;
        }

        const char* name = argv[2];
        const char* dname = argv[3];

        DiscoveryList::iterator iter;
        if (! DiscoveryTable::instance()->find(dname,&iter))
        {
            resultf("error adding announce %s to %s: "
                    "no such discovery agent",
                    name,dname);
            return TCL_ERROR;
        }

        Discovery* disc = *iter;
        if (! disc->announce(name,argc - 4,argv + 4))
        {
            resultf("error adding announce %s to %s",name,dname);
            return TCL_ERROR;
        }
        return TCL_OK;
    }
    else
    // discovery remove <name> <discovery name>
    if (strncasecmp("remove",argv[1],6) == 0)
    {
        if (argc != 4)
        {
            wrong_num_args(argc,argv,2,4,4);
            return TCL_ERROR;
        }

        const char* name = argv[2];
        const char* dname = argv[3];

        DiscoveryList::iterator iter;
        if (! DiscoveryTable::instance()->find(dname,&iter))
        {
            resultf("error removing announce %s from %s: no such discovery agent",
                    name,dname);
            return TCL_ERROR;
        }

        Discovery* disc = *iter;
        if (! disc->remove(name))
        {
            resultf("error removing announce %s from %s: no such announce",
                    name,dname);
            return TCL_ERROR;
        }
        return TCL_OK;
    }

    resultf("invalid discovery command: %s",argv[1]);
    return TCL_ERROR;
}

} // namespace dtn

#include "LinkCommand.h"
#include "bundling/Link.h"
#include "bundling/ContactManager.h"
#include <oasys/util/StringBuffer.h>


LinkCommand::LinkCommand()
    : TclCommand("link")
{
}

const char*
LinkCommand::help_string()
{
    return ""
        "link add <peer> <name>  <type> <convergence_layer> <args>\n"
        "      Note: <peer> is a bundle tuple pattern, <name> is any string \n"
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
        // link add  <nexthop> <name> <type> <clayer> <args>
        if (argc < 5) {
            wrong_num_args(argc, argv, 2, 3, INT_MAX);
            return TCL_ERROR;
        }
        
        const char* nexthop_str = argv[2];
        BundleTuplePattern nexthop(nexthop_str);
        if (!nexthop.valid()) {
            resultf("invalid next hop tuple %s", nexthop_str);
            return TCL_ERROR;
        }
       
        const char* name_str = argv[3];
        const char* type_str = argv[4];
        const char* cl = argv[5];
        
        Link::link_type_t type = Link::str_to_link_type(type_str);
        if (type == Link::LINK_INVALID) {
            resultf("invalid link type %s", type_str);
            return TCL_ERROR;
        }

        Link* link = ContactManager::instance()->find_link(name_str);
        if (link != NULL) {
            resultf("link name %s already exists, use different name", name_str);
            return TCL_ERROR;
        }
        std::string name(name_str);
        // XXX/Sushant pass other parameters?
        link = Link::create_link(name,type,cl,nexthop);
        
    }
    else {
        resultf("unimplemented link subcommand %s", cmd);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}

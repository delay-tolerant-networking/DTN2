
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

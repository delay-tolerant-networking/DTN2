
#include "BundleCommand.h"
#include "bundling/Bundle.h"

BundleCommand BundleCommand::instance_;

BundleCommand::BundleCommand() : AutoCommandModule("bundle") {}

int
BundleCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    // need a subcommand
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 2, INT_MAX);
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

    resultf("unknown bundle subcommand %s", cmd);
    return TCL_ERROR;
}


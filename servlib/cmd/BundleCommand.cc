
#include "BundleCommand.h"
#include "bundling/Bundle.h"
#include "bundling/BundleForwarding.h"

BundleCommand BundleCommand::instance_;

BundleCommand::BundleCommand() : AutoCommandModule("bundle") {}

char *
BundleCommand::helpString()
{
    return("inject <source> <dest> <payload>");
}

int
BundleCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    // need a subcommand
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 2, INT_MAX);
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "inject") == 0) {
        // bundle inject <source> <dest> <payload>
        if (argc != 5) {
            wrong_num_args(argc, argv, 2, 5, 5);
            return TCL_ERROR;
        }

        Bundle* b = new Bundle(argv[2], argv[3], argv[4]);
        BundleForwarding::instance()->input(b);
        return TCL_OK;
    } else {
        resultf("unknown bundle subcommand %s", cmd);
        return TCL_ERROR;
    }
}


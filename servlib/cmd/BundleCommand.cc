
#include "BundleCommand.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "routing/BundleRouter.h"

BundleCommand BundleCommand::instance_;

BundleCommand::BundleCommand() : AutoCommandModule("bundle") {}

const char*
BundleCommand::help_string()
{
    return("bundle inject <source> <dest> <payload>");
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
        // bundle inject <source> <dest> <payload> <length?>
        if (argc < 5 || argc > 6) {
            wrong_num_args(argc, argv, 2, 5, 6);
            return TCL_ERROR;
        }

        Bundle* b = new Bundle(argv[2], argv[3]);
        
        int len = strlen(argv[4]);
        if (argc == 5) {
            b->payload_.set_data(argv[4], len);
        } else {
            int total = atoi(argv[5]);
            b->payload_.set_length(total);
            b->payload_.append_data(argv[4], len);

            total -= len;
            for (; total > 0; --total) {
                b->payload_.mutable_data().push_back('.');
            }
        }
        
        BundleRouter::dispatch(new BundleReceivedEvent(b));
        return TCL_OK;
    } else {
        resultf("unknown bundle subcommand %s", cmd);
        return TCL_ERROR;
    }
}



#include "InterfaceCommand.h"
#include "bundling/InterfaceTable.h"

InterfaceCommand::InterfaceCommand() : AutoCommandModule("interface") {}

const char*
InterfaceCommand::help_string()
{
    return("\tinterface <tuple> [<args>?]\n"
           "\tinterface remove <tuple>");
}

int
InterfaceCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    // variants:
    // interface <tuple> [<args>?]
    // interface remove <tuple>
    
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 3, 4);
        return TCL_ERROR;
    }
    
    if (strcasecmp(argv[1], "remove") == 0) {
        // interface remove <tuple>
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }

        if (! InterfaceTable::instance()->del(argv[2])) {
            resultf("error removing interface %s", argv[2]);
            return TCL_ERROR;
        }
    } else {
        // interface <tuple> [args?]
        if (! InterfaceTable::instance()->add(argv[1], argc - 2, argv + 2)) {
            resultf("error adding interface %s", argv[1]);
            return TCL_ERROR;
        }
    }

    return TCL_OK;
}

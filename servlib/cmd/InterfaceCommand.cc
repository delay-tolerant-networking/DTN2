
#include "InterfaceCommand.h"
#include "bundling/Interface.h"

InterfaceCommand InterfaceCommand::instance_;

InterfaceCommand::InterfaceCommand() : AutoCommandModule("interface") {}

int
InterfaceCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    // variants:
    // interface <tuple> [args?]
    // interface remove <tuple>
    
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 3, 4);
        return TCL_ERROR;
    }
    
    if (strcasecmp(argv[1], "remove") == 0) {
        if (argc != 3) {
            wrong_num_args(argc, argv, 2, 3);
            return TCL_ERROR;
        }

        if (! Interface::del_interface(argv[2])) {
            resultf("error removing interface %s", argv[2]);
            return TCL_ERROR;
        }
    } else {
        if (! Interface::add_interface(argv[1], argc - 2, argv + 2)) {
            resultf("error adding interface %s", argv[1]);
            return TCL_ERROR;
        }
    }

    return TCL_OK;
}

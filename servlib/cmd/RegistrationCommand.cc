
#include "RegistrationCommand.h"
#include "reg/LoggingRegistration.h"

RegistrationCommand RegistrationCommand::instance_;

RegistrationCommand::RegistrationCommand() : AutoCommandModule("registration") {}

char *
RegistrationCommand::helpString()
{
    return("registration logger add <demux>\n\
\t\tregistration logger del <regid> <demux>");
}

int
RegistrationCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 3, 4);
        return TCL_ERROR;
    }
    const char* cmd = argv[1];

    if (strcmp(cmd, "logger") == 0) {
        // variants:
        // registration logger add <demux>
        // registration logger del <regid> <demux>
        if (argc < 4) {
            wrong_num_args(argc, argv, 2, 4, 5);
            return TCL_ERROR;
        }
        
        LoggingRegistration* reg;
        const char* op = argv[2];
        const char* demux = argv[3];

        if (strcmp(op, "add") == 0) {
            BundleTuple tuple(demux);
            if (!tuple.valid()) {
                resultf("error in registration logger add: invalid tuple %s", demux);
                return TCL_ERROR;
            }
            reg = new LoggingRegistration(demux);
            reg->start();
        }
        return TCL_OK;
    }

    return TCL_ERROR;
}

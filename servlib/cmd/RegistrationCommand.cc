
#include "RegistrationCommand.h"
#include "reg/LoggingRegistration.h"
#include "reg/TclRegistration.h"
#include "reg/RegistrationTable.h"
#include "storage/GlobalStore.h"

RegistrationCommand::RegistrationCommand() : AutoCommandModule("registration") {}

const char*
RegistrationCommand::help_string()
{
    return("\tregistration add <logger|tcl> <endpoint>\n"
           "\tregistration tcl <regid> <endpoint> <cmd> <args...>\n"
           "\tregistration del <regid> <endpoint>");
}

int
RegistrationCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    // need a subcommand
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 2, INT_MAX);
        return TCL_ERROR;
    }
    const char* op = argv[1];

    if (strcmp(op, "add") == 0) {
        // registration add <logger|tcl> <demux>
        if (argc != 4) {
            wrong_num_args(argc, argv, 2, 4, 5);
            return TCL_ERROR;
        }

        const char* type = argv[2];
        const char* demux_str = argv[3];
        BundleTuplePattern demux_tuple(demux_str);
        
        if (!demux_tuple.valid()) {
            resultf("error in registration add %s %s: invalid demux tuple",
                    type, demux_str);
            return TCL_ERROR;
        }

        u_int32_t regid = GlobalStore::instance()->next_regid();
        if (strcmp(type, "logger") == 0) {
            LoggingRegistration* reg;
            reg = new LoggingRegistration(regid, demux_tuple);
            reg->start();
        } else if (strcmp(type, "tcl") == 0) {
            TclRegistration* reg;
            reg = new TclRegistration(regid, demux_tuple, interp);
        } else {
            resultf("error in registration add %s %s: invalid type",
                    type, demux_str);
            return TCL_ERROR;
        }
        resultf("%d", regid);
        return TCL_OK;
    } else if (strcmp(op, "tcl") == 0) {
        // registration tcl <regid> <endpoint> <cmd> <args...>
        if (argc < 5) {
            wrong_num_args(argc, argv, 2, 5, INT_MAX);
            return TCL_ERROR;
        }

        const char* regid_str = argv[2];
        int regid = atoi(regid_str);
        const char* endpoint = argv[3];

        RegistrationTable* regtable = RegistrationTable::instance();
        TclRegistration* reg;
        reg = (TclRegistration*)regtable->get(regid, endpoint);

        if (!reg) {
            resultf("no matching registration for %s %s", regid_str, endpoint);
            return TCL_ERROR;
        }
        
        return reg->exec(argc - 4, &argv[4], interp);
    }

    resultf("invalid registration subcommand '%s'", op);
    return TCL_ERROR;
}


#include "StorageCommand.h"

StorageCommand StorageCommand::instance_;

StorageCommand::StorageCommand() : AutoCommandModule("storage")
{
}

void
StorageCommand::at_reg()
{
    bind_b("tidy", &tidy_);
    bind_s("dbdir", &dbdir_);
}

int
StorageCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    return TCL_ERROR;
}


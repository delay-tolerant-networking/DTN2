
#include "TestCommand.h"

TestCommand::TestCommand()
    : TclCommand("test"), id_(0)
{
}

void
TestCommand::bind_vars()
{
    bind_i("id", &id_);
    bind_s("initscript", &initscript_);
    bind_s("argv", &argv_);
}

int
TestCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    return TCL_ERROR;
}

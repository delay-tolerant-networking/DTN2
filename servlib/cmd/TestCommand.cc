
#include "TestCommand.h"

TestCommand::TestCommand()
    : TclCommand("test")
{
    bind_i("id", &id_, 0);
    bind_s("initscript", &initscript_);
}

int
TestCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    return TCL_ERROR;
}

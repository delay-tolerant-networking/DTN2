
#include "TestCommand.h"

TestCommand::TestCommand() :
    CommandModule("test"),
    loopback_(0)
{
    bind_i("loopback", &loopback_);
    bind_s("initscript", &initscript_);
}

int
TestCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    return TCL_ERROR;
}

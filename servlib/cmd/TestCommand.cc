
#include "TestCommand.h"

TestCommand::TestCommand() :
    CommandModule("test"),
    id_(0)
{
    bind_i("id", &id_);
    bind_s("initscript", &initscript_);
}

int
TestCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    return TCL_ERROR;
}

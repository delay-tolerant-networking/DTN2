
#include "LogCommand.h"
#include "debug/Log.h"

LogCommand LogCommand::instance_;

LogCommand::LogCommand() : AutoCommandModule("log") {}

int
LogCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    // log path level string
    if (argc != 4) {
        wrong_num_args(argc, argv, 1, 4);
        return TCL_ERROR;
    }
    
    ::logf(const_cast<char*>(argv[1]),
           str2level(const_cast<char*>(argv[2])), 
           const_cast<char*>(argv[3]));

    return TCL_OK;
}

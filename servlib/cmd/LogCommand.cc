
#include "LogCommand.h"
#include "debug/Log.h"

LogCommand LogCommand::instance_;

LogCommand::LogCommand() : AutoCommandModule("log") {}

char *
LogCommand::helpString()
{
    return("log path level string");
}

int
LogCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    // log path level string
    if (argc != 4) {
        wrong_num_args(argc, argv, 1, 4, 4);
        return TCL_ERROR;
    }

    log_level_t level = str2level(argv[2]);
    if (level == LOG_INVALID) {
        resultf("invalid log level %s", argv[2]);
        return TCL_ERROR;
    }
    
    ::logf(argv[1], level, argv[3]);

    return TCL_OK;
}

#include "TclUtil.h"

LogCommand LogCommand::instance_;

LogCommand::LogCommand() : AutoCommandModule("log") {}

void
LogCommand::exec(int argc, const char** args, 
                 Tcl_Interp* interp) throw(CommandError)
{
    // log path level string
    if (argc != 4) {
        throw CommandError("wrong number of args");
    }
    
    ::logf(const_cast<char*>(args[1]),
           str2lev(const_cast<char*>(args[2])), 
           const_cast<char*>(args[3]));
}

log_level_t
LogCommand::str2lev(char* str)
{
    if(strcmp(str, "DEBUG") == 0)
    {
        return LOG_DEBUG;
    } 
    else if(strcmp(str, "INFO") == 0) 
    {
        return LOG_INFO;
    }
    else if(strcmp(str, "WARN") == 0)
    {
        return LOG_WARN;
    }
    else if(strcmp(str, "ERR") == 0) 
    {
        return LOG_ERR;
    }
    else if(strcmp(str, "CRIT") == 0) 
    {
        return LOG_CRIT;
    }

    throw CommandError("Unknown error level");
}

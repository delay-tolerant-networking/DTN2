#ifndef __TCL_UTIL_H__
#define __TCL_UTIL_H__

#include "Log.h"
#include "Command.h"

/// \file 
///
/// General Tcl command definitions to hook up to parts of the Tier
/// system. The purpose of this file is to keep dependencies of the
/// ConfigModules from other reusable parts, like the logging system
/// seperate.
///
class LogCommand : public AutoCommandModule {
public:
    LogCommand();
    
    // virtual from CommandModule
    virtual void exec(int argc, const char** args, 
                      Tcl_Interp* interp) throw(CommandError);

protected:
    log_level_t str2lev(char* str);

    static LogCommand instance_;
};


#endif //__TCL_UTIL_H__

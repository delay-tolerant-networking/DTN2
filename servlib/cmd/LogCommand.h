#ifndef _LOG_COMMAND_H_
#define _LOG_COMMAND_H_

#include "Command.h"

/**
 * Class to export the logging system to tcl scripts.
 */
class LogCommand : public AutoCommandModule {
public:
    LogCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);

    static void foo() {}
        
protected:
    static LogCommand instance_;
};


#endif //_LOG_COMMAND_H_

#ifndef _HELP_COMMAND_H_
#define _HELP_COMMAND_H_

#include "Command.h"

/**
 * CommandModule for the "help" command.
 */
class HelpCommand : public AutoCommandModule {
public:
    HelpCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    virtual char *help_string();

protected:
    static HelpCommand instance_;
};

#endif /* _HELP_COMMAND_H_ */

#ifndef _QUIT_COMMAND_H_
#define _QUIT_COMMAND_H_

#include "Command.h"

/**
 * CommandModule for the "interface" command.
 */
class QuitCommand : public AutoCommandModule {
public:
    QuitCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    char *help_string();

protected:
    static QuitCommand instance_;
};

#endif /* _QUIT_COMMAND_H_ */

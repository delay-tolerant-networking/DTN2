#ifndef _INTERFACE_COMMAND_H_
#define _INTERFACE_COMMAND_H_

#include "Command.h"

/**
 * CommandModule for the "interface" command.
 */
class InterfaceCommand : public AutoCommandModule {
public:
    InterfaceCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    virtual const char* help_string();

protected:
    static InterfaceCommand instance_;
};

#endif /* _INTERFACE_COMMAND_H_ */

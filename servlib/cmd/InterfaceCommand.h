#ifndef _INTERFACE_COMMAND_H_
#define _INTERFACE_COMMAND_H_

#include "tclcmd/TclCommand.h"

/**
 * CommandModule for the "interface" command.
 */
class InterfaceCommand : public TclCommand {
public:
    InterfaceCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    virtual const char* help_string();
};

#endif /* _INTERFACE_COMMAND_H_ */

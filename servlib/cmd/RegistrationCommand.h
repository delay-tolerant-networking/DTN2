#ifndef _REGISTRATION_COMMAND_H_
#define _REGISTRATION_COMMAND_H_

#include "tclcmd/TclCommand.h"

/**
 * The "registration" command.
 */
class RegistrationCommand : public TclCommand {
public:
    RegistrationCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    virtual const char* help_string();
};

#endif /* _REGISTRATION_COMMAND_H_ */

#ifndef _REGISTRATION_COMMAND_H_
#define _REGISTRATION_COMMAND_H_

#include "Command.h"

/**
 * CommandModule for the "registration" command.
 */
class RegistrationCommand : public AutoCommandModule {
public:
    RegistrationCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    virtual const char* help_string();

protected:
    static RegistrationCommand instance_;
};

#endif /* _REGISTRATION_COMMAND_H_ */

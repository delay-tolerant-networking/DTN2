#ifndef _PARAM_COMMAND_H_
#define _PARAM_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

/**
 * Parameter setting command
 */
class ParamCommand : public TclCommand {
public:
    ParamCommand();
    
    /**
     * Virtual from CommandModule.
     */
    const char* help_string();
};


#endif /* _PARAM_COMMAND_H_ */

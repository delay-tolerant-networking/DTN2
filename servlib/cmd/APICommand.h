#ifndef _API_COMMAND_H_
#define _API_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

/**
 * API options command
 */
class APICommand : public TclCommand {
public:
    APICommand();
    
    /**
     * Virtual from CommandModule.
     */
    const char* help_string();
};


#endif /* _API_COMMAND_H_ */

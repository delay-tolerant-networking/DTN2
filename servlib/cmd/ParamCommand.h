#ifndef _PARAM_COMMAND_H_
#define _PARAM_COMMAND_H_

#include "Command.h"

/**
 * Parameter setting command
 */
class ParamCommand : public AutoCommandModule {
public:
    ParamCommand();
    
    /**
     * Virtual from CommandModule.
     */
    void at_reg();
    const char* help_string();

protected:
    static ParamCommand instance_;
};


#endif /* _PARAM_COMMAND_H_ */

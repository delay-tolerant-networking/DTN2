#ifndef _API_COMMAND_H_
#define _API_COMMAND_H_

#include "Command.h"

/**
 * Apieter setting command
 */
class APICommand : public AutoCommandModule {
public:
    APICommand();
    
    /**
     * Virtual from CommandModule.
     */
    void at_reg();
    const char* help_string();

protected:
    static APICommand instance_;
};


#endif /* _API_COMMAND_H_ */

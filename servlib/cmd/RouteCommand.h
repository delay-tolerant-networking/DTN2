#ifndef _ROUTE_COMMAND_H_
#define _ROUTE_COMMAND_H_

#include "Command.h"

/**
 * CommandModule for the "route" command.
 */
class RouteCommand : public AutoCommandModule {
public:
    RouteCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    virtual const char* help_string();

protected:
    static RouteCommand instance_;
};

#endif /* _ROUTE_COMMAND_H_ */

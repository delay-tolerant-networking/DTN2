#ifndef _ROUTE_COMMAND_H_
#define _ROUTE_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

/**
 * The "route" command.
 */
class RouteCommand : public TclCommand {
public:
    RouteCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    virtual const char* help_string();
};

#endif /* _ROUTE_COMMAND_H_ */

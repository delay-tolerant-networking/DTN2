#ifndef _LINK_COMMAND_H_
#define _LINK_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

/**
 * The "link" command.
 */
class LinkCommand : public TclCommand {
public:
    LinkCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    virtual const char* help_string();
};

#endif /* _LINK_COMMAND_H_ */

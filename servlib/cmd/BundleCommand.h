#ifndef _BUNDLE_COMMAND_H_
#define _BUNDLE_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

/**
 * Debug command for hand manipulation of bundles.
 */
class BundleCommand : public TclCommand {
public:
    BundleCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int objc, Tcl_Obj** objv, Tcl_Interp* interp);
    virtual const char* help_string();
};

#endif /* _BUNDLE_COMMAND_H_ */

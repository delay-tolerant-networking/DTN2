#ifndef _BUNDLE_COMMAND_H_
#define _BUNDLE_COMMAND_H_

#include "Command.h"

/**
 * Debug command for hand manipulation of bundles.
 */
class BundleCommand : public AutoCommandModule {
public:
    BundleCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    virtual char *help_string();

protected:
    static BundleCommand instance_;
};

#endif /* _BUNDLE_COMMAND_H_ */

#ifndef _TEST_COMMAND_H_
#define _TEST_COMMAND_H_

// XXX/demmer this really belongs in the daemon directory...

#include <oasys/tclcmd/TclCommand.h>

/**
 * CommandModule for the "test" command.
 */
class TestCommand : public TclCommand {
public:
    TestCommand();

    /**
     * Binding function. Since the class is created before logging is
     * initialized, this can't be in the constructor.
     */
    void bind_vars();

    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    
    int id_;			///< sets the test node id
    std::string initscript_;	///< tcl script to run at init
    std::string argv_;		///< "list" of space-separated args
};

#endif /* _TEST_COMMAND_H_ */

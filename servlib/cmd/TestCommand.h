#ifndef _TEST_COMMAND_H_
#define _TEST_COMMAND_H_

#include "tclcmd/TclCommand.h"

/**
 * CommandModule for the "test" command.
 */
class TestCommand : public TclCommand {
public:
    TestCommand();

    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    
    int id_;			///< sets the test node id
    std::string initscript_;	///< tcl script to run at init
};

#endif /* _TEST_COMMAND_H_ */

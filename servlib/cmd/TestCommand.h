#ifndef _TEST_COMMAND_H_
#define _TEST_COMMAND_H_

#include "Command.h"

/**
 * CommandModule for the "test" command.
 */
class TestCommand : public CommandModule {
public:
    TestCommand();

    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    
    int loopback_;	///< for testing purposes, sets the loopback node id
};

#endif /* _TEST_COMMAND_H_ */

#ifndef _SIM_DTN2_COMMAND_H_
#define _SIM_DTN2_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

/**
 * CommandModule for the "simdtn2" command.
 * These are the commands specific to DTN2 and sim
 * For example commands to access bundlerouterifo from simulator
 */
class Simdtn2Command : public AutoTclCommand {
public:
    Simdtn2Command();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    void at_reg();
    virtual const char* help_string();

protected:
    static Simdtn2Command instance_;
};

#endif /* _SIM_DTN2_COMMAND_H_ */

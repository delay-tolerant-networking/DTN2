#ifndef _SIM_DTN2_COMMAND_H_
#define _SIM_DTN2_COMMAND_H_

#include "cmd/Command.h"

/**
 * CommandModule for the "simroute" command.
 */
class Simdtn2Command : public AutoCommandModule {
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

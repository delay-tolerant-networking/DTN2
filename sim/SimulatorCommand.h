#ifndef _SIMULATOR_COMMAND_H_
#define _SIMULATOR_COMMAND_H_

#include "cmd/Command.h"

/**
 * Class to control the simulator
 */
class SimulatorCommand : public AutoCommandModule {
public:
    SimulatorCommand()  ;
    void at_reg();
    const char* help_string();
    static SimulatorCommand* instance() { return &instance_; }
   
    // simtime_t runtill_ ; 
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);

protected:
    static SimulatorCommand instance_;
 };


#endif /* _SIMULATOR_COMMAND_H_ */

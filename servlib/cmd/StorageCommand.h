#ifndef _STORAGE_COMMAND_H_
#define _STORAGE_COMMAND_H_

#include "tclcmd/TclCommand.h"

/**
 * Class to control the storage system.
 */
class StorageCommand : public TclCommand {
public:    
    StorageCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
    
protected:
    bool inited_;
};


#endif /* _STORAGE_COMMAND_H_ */

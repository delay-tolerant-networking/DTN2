#ifndef _STORAGE_COMMAND_H_
#define _STORAGE_COMMAND_H_

#include "Command.h"

/**
 * Class to control the storage system.
 */
class StorageCommand : public AutoCommandModule {
public:
    StorageCommand();
    void at_reg();
    static StorageCommand* instance() { return &instance_; }
    
    /**
     * Public configuration variables.
     */
    bool tidy_;
    std::string dbdir_;
    std::string sqldb_;

    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);

protected:
    static StorageCommand instance_;
    bool inited_;
};


#endif /* _STORAGE_COMMAND_H_ */


#include "StorageCommand.h"
#include "storage/SQLBundleStore.h"
#include "storage/SQLGlobalStore.h"
#include "storage/SQLRegistrationStore.h"
#include "storage/MysqlSQLImplementation.h"
#include "storage/PostgresSQLImplementation.h"

StorageCommand StorageCommand::instance_;

StorageCommand::StorageCommand() : AutoCommandModule("storage")
{
    inited_ = false;
}

void
StorageCommand::at_reg()
{
    bind_b("tidy", &tidy_);
    bind_s("dbdir", &dbdir_);
    bind_s("sqldb", &sqldb_, "dtn");
}

int
StorageCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    // need a subcommand
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 2, INT_MAX);
        return TCL_ERROR;
    }

    const char* cmd = argv[1];
    
    if (strcmp(cmd, "init") == 0) {
        if (inited_) {
            resultf("storage init already called");
            return TCL_ERROR;
        }
        
        // storage init <type>
        if (argc < 3) {
            wrong_num_args(argc, argv, 2, 3, 3);
            return TCL_ERROR;
        }

        const char* type = argv[2];
        SQLImplementation* impl;
        
        if ((strcmp(type, "mysql") == 0) ||
            (strcmp(type, "postgres") == 0))
        {
            if (strcmp(type, "mysql") == 0) {
                impl = new MysqlSQLImplementation();
            } else {
                impl = new PostgresSQLImplementation();
            }
            
            if (impl->connect(sqldb_.c_str()) == -1) {
                resultf("error connecting to database %s", sqldb_.c_str());
                return TCL_ERROR;
            }

            GlobalStore::init(new SQLGlobalStore(impl));
            BundleStore::init(new SQLBundleStore(impl));
            RegistrationStore::init(new SQLRegistrationStore(impl));
            return TCL_OK;

        } else {
            resultf("storage type %s not implemented", type);
            return TCL_ERROR;
        }
    }
        
    return TCL_OK;
}



#include "StorageCommand.h"
#include "storage/StorageConfig.h"
#include "storage/BerkeleyDBStore.h"
#include "storage/BerkeleyDBBundleStore.h"
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
    StorageConfig* cfg = StorageConfig::instance();
    bind_b("tidy", &cfg->tidy_);
    bind_s("dbdir", &cfg->dbdir_);
    bind_s("sqldb", &cfg->sqldb_, "dtn");
}

int
StorageCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    // need a subcommand
    if (argc < 2) {
        wrong_num_args(argc, argv, 1, 2, INT_MAX);
        return TCL_ERROR;
    }

    StorageConfig* cfg = StorageConfig::instance();
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

        GlobalStore* global_store;
        BundleStore* bundle_store;
        RegistrationStore* reg_store;
        
        if ((strcmp(type, "mysql") == 0) ||
            (strcmp(type, "postgres") == 0))
        {
            if (strcmp(type, "mysql") == 0) {
                impl = new MysqlSQLImplementation();
            } else {
                impl = new PostgresSQLImplementation();
            }
            
            if (impl->connect(cfg->sqldb_.c_str()) == -1) {
                resultf("error connecting to database %s",
                        cfg->sqldb_.c_str());
                return TCL_ERROR;
            }

            global_store = new SQLGlobalStore(impl);
            bundle_store = new SQLBundleStore(impl);
            reg_store    = new SQLRegistrationStore(impl);

        } else if (strcmp(type, "berkeleydb") == 0) {
            BerkeleyDBManager::instance()->init();
//            global_store = new BerkeleyDBGlobalStore()
            bundle_store = new BerkeleyDBBundleStore();
//            reg_store    = new BerkeleyDBRegistrationStore();
            
        } else {
            resultf("storage type %s not implemented", type);
            return TCL_ERROR;
        }

        GlobalStore::init(global_store);
        BundleStore::init(bundle_store);
        RegistrationStore::init(reg_store);
    }
        
    return TCL_OK;
}


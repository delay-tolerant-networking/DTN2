
#include "StorageCommand.h"
#include "storage/SQLBundleStore.h"
#include "storage/PostgresSQLImplementation.h"

StorageCommand StorageCommand::instance_;

StorageCommand::StorageCommand() : AutoCommandModule("storage")
{
}

void
StorageCommand::at_reg()
{
    bind_b("tidy", &tidy_);
    bind_s("dbdir", &dbdir_);
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
        // storage init [db | file | mysql | postgres] <args?>
        if (argc < 3) {
            wrong_num_args(argc, argv, 2, 3, 4);
            return TCL_ERROR;
        }

        const char* type = argv[2];

        if (strcmp(type, "db") == 0) {
            PANIC("berkeley db storage not implemented");
        }
        else if (strcmp(type, "file") == 0) {
            PANIC("file storage not implemented");
        }
        else if (strcmp(type, "mysql") == 0) {
            PANIC("mysql storage not implemented");
        }
        else if (strcmp(type, "postgres") == 0) {
            SQLImplementation* impl = new PostgresSQLImplementation("dtn");
            BundleStore::init(new SQLBundleStore("bundles", impl));
        } else {
            resultf("unknown storage type %s", type);
            return TCL_ERROR;
        }

        return TCL_OK;
    }
    return TCL_ERROR;
}


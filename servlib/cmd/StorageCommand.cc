
#include "StorageCommand.h"
#include "storage/StorageConfig.h"

StorageCommand::StorageCommand()
    : TclCommand("storage")
{
    inited_ = false;

    StorageConfig* cfg = StorageConfig::instance();
    bind_s("type",  &cfg->type_);
    bind_b("tidy",  &cfg->tidy_);
    bind_s("dbdir", &cfg->dbdir_);
    bind_s("sqldb", &cfg->sqldb_);
}

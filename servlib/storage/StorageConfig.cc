
#include "StorageConfig.h"

StorageConfig StorageConfig::instance_;

StorageConfig::StorageConfig()
{
    tidy_	= true;
    dbdir_	= "/var/bundles/db";
    dbfile_ 	= "DTN.db";
    dberrlog_	= "error.log";
    sqldb_ 	= "DTN";
}

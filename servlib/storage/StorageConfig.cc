
#include "StorageConfig.h"

StorageConfig StorageConfig::instance_;

StorageConfig::StorageConfig()
{
    tidy_	= false;
    dbdir_	= "/var/bundles/db";
    dbfile_ 	= "DTN.db";
    dberrlog_	= "error.log";
    sqldb_ 	= "DTN";
}

#ifndef _STORAGE_CONFIG_H_
#define _STORAGE_CONFIG_H_

#include <string>
/**
 * Simple singleton class that just contains the storage-specific
 * configuration variables.
 */
class StorageConfig {
public:
    StorageConfig();
    static StorageConfig* instance() { return &instance_; }
    
    std::string type_;		///< storage type [berkeleydb/mysql/postgres]
    bool tidy_;			///< Prune out the database on init
    int tidy_wait_;		///< seconds to wait before tidying
    std::string dbdir_;		///< Path to the database files
    std::string dbfile_;	///< Actual db file name
    std::string dberrlog_;	///< DB internal error log file
    std::string sqldb_;		///< Database name within sql

protected:
    static StorageConfig instance_;
};

#endif /* _STORAGE_CONFIG_H_ */

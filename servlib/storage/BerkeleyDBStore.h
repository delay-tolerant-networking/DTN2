#ifndef _BERKELEY_DB_STORE_H_
#define _BERKELEY_DB_STORE_H_

#include <db_cxx.h>
#include "PersistentStore.h"
#include <oasys/debug/Log.h>

class BerkeleyDBManager;
class SerializableObject;

/**
 * Singleton class to manage the overall db environment.
 */
class BerkeleyDBManager : public Logger {
public:
    BerkeleyDBManager();
    static BerkeleyDBManager* instance() { return &instance_; }
    
    void init();

    Db* open_table(const char* tablename);
    
protected:
    static BerkeleyDBManager instance_;
    FILE*  errlog_;             ///< db err log file
    DbEnv* dbenv_;              ///< environment for all tables
};

/**
 * Generic implementation of a StorageManager that uses a table in an
 * underlying BerkeleyDB database.
 */
class BerkeleyDBStore : public PersistentStore, public Logger {
public:
    BerkeleyDBStore(const char* table);
    virtual ~BerkeleyDBStore();

    /// @{ Virtual overrides from PersistentStore
    int exists(const int key);
    int get(SerializableObject* obj, const int key);
    int add(SerializableObject* obj, const int key);
    int update(SerializableObject* obj, const int key);
    int del(const int key);
    int num_elements();
    void keys(std::vector<int> * v);
    void elements(std::vector<SerializableObject*> v);
    /// @}

protected:
    std::string tablename_;
    Db* db_;
};

#endif /* _BERKELEY_DB_STORE_H_ */

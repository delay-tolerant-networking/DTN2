#ifndef __DB_DISABLED__

#include "BerkeleyDBStore.h"
#include "StorageConfig.h"
#include "serialize/MarshalSerialize.h"
#include "util/StringBuffer.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <unistd.h>

#define NO_TX  0 // for easily going back and changing TX id's later

BerkeleyDBManager BerkeleyDBManager::instance_;

BerkeleyDBManager::BerkeleyDBManager()
    : Logger("/storage/berkeleydb")
{
}

void
BerkeleyDBManager::init()
{
    StorageConfig* cfg = StorageConfig::instance();

    const char* dbdir = cfg->dbdir_.c_str();

    log_debug("initializing berkeley db manager with dbdir %s", dbdir);

    // create database directory
    struct stat st;
    if (stat(dbdir, &st) == -1) {
        if (errno == ENOENT) {
            log_info("creating new database directory '%s'", dbdir);
        }
        
        if (mkdir(dbdir, 0700) != 0) {
            log_crit("can't create datastore directory %s: %s",
                     dbdir, strerror(errno));
            exit(-1);
        }
    }

    // create the db environment
    dbenv_  = new DbEnv(0);

    // set up db internal error log file
    StringBuffer errlog("%s/%s", dbdir, cfg->dberrlog_.c_str());
    
    errlog_ = ::fopen(errlog.c_str(), "w");
    if (!errlog_) {
        log_err("error creating db error log file %s: %s",
                errlog.c_str(), strerror(errno));
    } else {
        dbenv_->set_errfile(errlog_);
    }

    // Commented  by XXX Sushant
    /*
    // handle the tidy option
    if (cfg->tidy_) {
        char cmd[256];
        sprintf(cmd, "/bin/rm -rf %s", dbdir);
        log_warn("pruning contents of db dir: '%s'", cmd);
        system(cmd);
        if (mkdir(dbdir, 0700) != 0) {
            log_crit("can't create datastore directory %s: %s (XXX continuing anyway)",
                     dbdir, strerror(errno));
            //exit(-1);
        }
    }
    */


    // try to open the db file
    try {
        dbenv_->open(dbdir,
                     DB_CREATE     | // create new files
                     DB_INIT_MPOOL | // initialize memory pool
                     DB_INIT_LOG   | // use logging
                     DB_INIT_TXN   | // use transactions
                     DB_RECOVER    | // recover from previous crash (if any)
                     DB_PRIVATE,     // only one process can access the db
                     0);             // no flags

        dbenv_->set_flags(DB_AUTO_COMMIT, // every op is automatically in a tx
                          1);
    } catch(DbException e) {
        log_crit("DB error: %s", e.what());
        PANIC("cannot open database!");
    }

    // Create database file if none exists. Table 0 is used for
    // storing general global metadata.
    StringBuffer dbpath("%s/%s", dbdir, cfg->dbfile_.c_str());
    
    if (stat(dbpath.c_str(), &st) == -1) {
        if(errno == ENOENT) {
            Db db(dbenv_, 0);
            log_info("creating new database file '%s'", cfg->dbfile_.c_str());
            
            try {
                db.open(NO_TX, cfg->dbfile_.c_str(), "0", DB_HASH, DB_CREATE, 0);
                db.close(0);
            } 
            catch(DbException e) 
            {
                log_crit("DB: %s", e.what());
                PANIC("can't create database file");
            }
        }
        else {
            PANIC("Unable to stat file: %s", strerror(errno));
        }
    }
}

Db*
BerkeleyDBManager::open_table(const char* tablename)
{
    StorageConfig* cfg = StorageConfig::instance();
    Db* db = new Db(dbenv_, 0);

    u_int flags = 0;
    if (cfg->tidy_) {
        flags = DB_CREATE;
    }
    try {
        db->open(NO_TX, cfg->dbfile_.c_str(), tablename, DB_HASH, flags, 0);
    }
    catch(DbException e) {
        log_err("Error opening table %s: %s", tablename, e.what());
        return 0;
    }

    log_debug("opened table %s", tablename);

    return db;
}

BerkeleyDBStore::BerkeleyDBStore(const char* tablename)
    : tablename_(tablename)
{
    logpathf("/storage/berkeleydb/%s", tablename);
    BerkeleyDBManager* mgr = BerkeleyDBManager::instance();
    db_ = mgr->open_table(tablename);
    ASSERT(db_);
}

BerkeleyDBStore::~BerkeleyDBStore()
{
    NOTIMPLEMENTED;
}

int
BerkeleyDBStore::get(SerializableObject* obj, const int key)
{
    char keybuf[32];
    int keylen = snprintf(keybuf, sizeof(keybuf), "%u", (unsigned int)key);
    
    Dbt k((void*)keybuf, keylen);
    Dbt d;
    int err;

    try {
        err = db_->get(NO_TX, &k, &d, 0);
    } catch(DbException e) {
        PANIC("error in DB get: %s", e.what());
    }
    
    if (err == DB_NOTFOUND) {
        log_warn("get key %d failed", key);
        return -1;
    }

    Unmarshal unmarshal((u_char*)d.get_data(), d.get_size());

    if (unmarshal.action(obj) < 0) {
        PANIC("error in unserialize");
    }

    log_debug("get key %d success, initialized %p", key, obj);
    
    return 0;
}

int
BerkeleyDBStore::put(SerializableObject* obj, const int key)
{
    // figure out the flattened size of the object
    MarshalSize marshalsize;    
    int ret = marshalsize.action(obj);
    ASSERT(ret == 0);
    size_t size = marshalsize.size();
    ASSERT(size > 0);

    // do the flattening
    u_char* buf = (u_char*)malloc(size);
    Marshal marshal(buf, size);
    ret = marshal.action(obj);
    ASSERT(ret == 0);

    // now cons up a db key/value pair
    char keybuf[32];
    int keylen = snprintf(keybuf, sizeof(keybuf), "%u", (unsigned int)key);
    
    Dbt k(keybuf, keylen);
    Dbt d(buf, size);

    // safety check
    if (size > 1024) {
        log_warn("unusual large db put length %d (table %s key %d)",
                 size, tablename_.c_str(), key);
    }

    log_debug("put key %d, obj %p success", key, obj);
    
    int err;
    try {
        err = db_->put(NO_TX, &k, &d, 0);
    } catch(DbException e) {
        PANIC("DB: %s", e.what());
    }

    return 0;
}

int
BerkeleyDBStore::del(const int key)
{
    char keybuf[32];
    int keylen = snprintf(keybuf, sizeof(keybuf), "%u", (unsigned int)key);
    Dbt k(keybuf, keylen);
    
    try {
        if (db_->del(NO_TX, &k, 0) == DB_NOTFOUND) {
            return -1;
        }
    } catch(DbException e) {
        PANIC("DB: %s", e.what());
    }
    
    return 0;
}

int
BerkeleyDBStore::num_elements()
{
    NOTIMPLEMENTED;
}

void
BerkeleyDBStore::keys(std::vector<int> v)
{
    NOTIMPLEMENTED;
}

void
BerkeleyDBStore::elements(std::vector<SerializableObject*> v)
{
    NOTIMPLEMENTED;
}

#endif /* __DB_DISABLED__ */

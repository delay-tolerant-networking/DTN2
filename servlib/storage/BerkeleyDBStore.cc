
#include "config.h"

#if __DB_ENABLED__

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <unistd.h>

#include <oasys/serialize/MarshalSerialize.h>
#include <oasys/util/StringBuffer.h>

#include "BerkeleyDBStore.h"
#include "StorageConfig.h"

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

    int initflags = 0;

    log_debug("initializing berkeley db manager with dbdir %s tidy=%d init=%d", dbdir, cfg->tidy_, cfg->init_);

    // Init Option: create database directory, initially must not exist
    if (cfg->init_)
    {
        initflags |= DB_CREATE;
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


    if (cfg->init_)
    {
        // Create database file if none exists. Table 0 is used for
        // storing general global metadata.
        StringBuffer dbpath("%s/%s", dbdir, cfg->dbfile_.c_str());
        
        struct stat st;
        if (stat(dbpath.c_str(), &st) == -1) {
            if(errno == ENOENT) {
                Db db(dbenv_, 0);
                log_info("creating new database file '%s'", cfg->dbfile_.c_str());
                
                try {
                    db.open(NO_TX, cfg->dbfile_.c_str(), "0", DB_BTREE, DB_CREATE, 0);
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
}

Db*
BerkeleyDBManager::open_table(const char* tablename)
{
    StorageConfig* cfg = StorageConfig::instance();
    Db* db = new Db(dbenv_, 0);

    u_int flags = 0;
    if (cfg->tidy_ || cfg->init_) {
        flags = DB_CREATE;
    }

    try {
        db->open(NO_TX, cfg->dbfile_.c_str(), tablename, DB_BTREE, flags, 0);
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
BerkeleyDBStore::exists(const int key)
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
        return false;
    }
    else if (err == 0)
    {
        return true;
    }
    else
    {
        PANIC("error %d in DB exists", err);
    }
}

int
BerkeleyDBStore::get(SerializableObject* obj, const int key)
{
    Dbt k;
    int dbkey = key;
    k.set_data(&dbkey);
    k.set_size(sizeof(key));
    k.set_ulen(sizeof(key));
    k.set_flags(DB_DBT_USERMEM);

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

    Unmarshal unmarshal(SerializeAction::CONTEXT_LOCAL,
                        (u_char*)d.get_data(), d.get_size());

    if (unmarshal.action(obj) < 0) {
        PANIC("error in unserialize");
    }

    log_debug("get key %d success, initialized %p", key, obj);
    
    return 0;
}

int
BerkeleyDBStore::add(SerializableObject* obj, const int key)
{
    //    del(key);
 
    // figure out the flattened size of the object
    MarshalSize marshalsize(SerializeAction::CONTEXT_LOCAL);
    int ret = marshalsize.action(obj);
    ASSERT(ret == 0);
    size_t size = marshalsize.size();
    ASSERT(size > 0);

    // do the flattening
    u_char* buf = (u_char*)malloc(size);
    Marshal marshal(SerializeAction::CONTEXT_LOCAL,
                    buf, size);
    ret = marshal.action(obj);
    ASSERT(ret == 0);
    
    Dbt k;
    int dbkey = key;
    bzero(&k, sizeof(DBT));
    k.set_data(&dbkey);
    k.set_size(sizeof(dbkey));
    k.set_ulen(sizeof(dbkey));
    k.set_flags(DB_DBT_USERMEM);

    Dbt d;
    bzero(&d, sizeof(DBT));
    d.set_data(buf);
    d.set_size(size);
    d.set_ulen(size);
    d.set_flags(DB_DBT_USERMEM);

    // safety check
    if (size > 1024) {
        log_warn("unusual large db put length %d (table %s key %d)",
                 size, tablename_.c_str(), key);
    }

    int err;
    try {
        err = db_->put(NO_TX, &k, &d, 0);
    } catch(DbException e) {
        PANIC("DB: %s", e.what());
    }

    log_debug("put key %d, obj %p success", key, obj);

    return 0;
}

int
BerkeleyDBStore::update(SerializableObject* obj, const int key)
{
    // del(key);
    return add(obj, key);
}

int
BerkeleyDBStore::del(const int key)
{
    int dbkey = key;
    Dbt k(&dbkey, sizeof(dbkey));
    
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
    DB_BTREE_STAT * sp;

    try {
        if (db_->stat(&sp, 0) != 0)
        {
            PANIC("DB: error counting elements in database!");
        }

        return (int) sp->bt_nkeys;
    } catch(DbException e) {
        PANIC("DB: %s", e.what());
    }
        
    return 0;
}

void
BerkeleyDBStore::keys(std::vector<int> * v)
{

    // placeholders for key information
    char keybuf[32];
    int keylen = sizeof(keybuf);
    u_int32_t key = 0;
    bzero(keybuf, keylen);

    //    char obj[102400];


    // Database structures
    Dbc * cursor;

    Dbt k;
    k.set_data(&key);
    k.set_size(sizeof(key));
    k.set_ulen(sizeof(key));
    k.set_flags(DB_DBT_USERMEM);

    Dbt d;

    try {
        if (db_->cursor(NULL, &cursor, 0) != 0)
        {
            PANIC("DB: error getting cursor!");
        }

        while (cursor->get(&k, &d, DB_NEXT) == 0)
        {
            v->push_back(key);
        }

        cursor->close();

    } catch(DbException e) {
        PANIC("DB: %s", e.what());
    }

    
}
#endif /* __DB_ENABLED__ */

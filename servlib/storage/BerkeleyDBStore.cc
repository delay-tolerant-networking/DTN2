/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

    log_debug("initializing berkeley db manager with dbdir %s tidy=%d init=%d",
              dbdir, cfg->tidy_, cfg->init_);

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
    
    // open the db environment -- doesn't actually create the database
    // but will run crash recovery (so needs DB_CREATE)
    try {
        dbenv_->open(dbdir,
                     DB_CREATE     | // create (required for recovery)
                     DB_INIT_MPOOL | // initialize memory pool
                     DB_INIT_LOG   | // use logging
                     DB_INIT_TXN   | // use transactions
                     DB_RECOVER    | // recover from previous crash (if any)
                     DB_PRIVATE,     // only one process can access the db
                     0);
        
        dbenv_->set_flags(DB_AUTO_COMMIT, 1); // every op automatically in a tx
    } catch(DbException e) {
        log_crit("DB error opening db environment: %s", e.what());
        exit(1);
    }

    // make sure the database file exists or create it if init_ is set
    StringBuffer dbpath("%s/%s", dbdir, cfg->dbfile_.c_str());

    struct stat st;
    log_debug("checking database file '%s'...", cfg->dbfile_.c_str());
    int ret = stat(dbpath.c_str(), &st);
    int flags = 0;

    if (ret == -1 && errno != ENOENT)
    {
        log_crit("error trying to stat database file '%s': %d (%s)",
                 dbpath.c_str(), ret, strerror(errno));
        exit(1);
    }

    if (cfg->init_) {
        if ((ret == -1) && (errno == ENOENT))
        {
            flags = DB_CREATE; // ok 
        }
        else if (ret == 0)
        {
            log_err("database file '%s' exists but --init-db specified -- "
                    "remove the database file or re-run without --init-db",
                    dbpath.c_str());
            exit(1);
        }
    } else { // !init_
        if ((ret == 0) && S_ISREG(st.st_mode))
        {
            flags = 0; // ok
        }
        else if ((ret == 0) && !S_ISREG(st.st_mode))
        {
            log_err("database file '%s' is not a regular file -- "
                    "remove and rerun with --init-db to create the database",
                    dbpath.c_str());
            exit(1);
        }
        else if ((ret == -1) && (errno == ENOENT))
        {
            log_err("database file '%s' does not exist -- "
                    "rerun with --init-db to create the database",
                    dbpath.c_str());
            exit(1);
        }
    }

    // now try to open a dummy table to make sure the database exists
    // and is valid (or create it if init is set)
    try {
        Db db(dbenv_, 0);
        db.open(NO_TX, cfg->dbfile_.c_str(), "__dtn__", DB_BTREE, flags, 0);
        db.close(0);
    } catch(DbException e) {
        log_crit("error creating or opening database file '%s': %s",
                 dbpath.c_str(), e.what());
        exit(1);
    }
    
    log_info("database file '%s' validated", dbpath.c_str());
}

Db*
BerkeleyDBManager::open_table(const char* tablename)
{
    StorageConfig* cfg = StorageConfig::instance();
    Db* db = new Db(dbenv_, 0);

    u_int flags = 0;
    if (cfg->tidy_ || cfg->init_) {
        log_info("initializing %s database table...", tablename);
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
    StringBuffer buf("/store/%s", tablename_.c_str());

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
    StringBuffer logbuf("/store/%s", tablename_.c_str());
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

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

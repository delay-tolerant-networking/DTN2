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
#ifndef _SQL_STORE_H_
#define _SQL_STORE_H_

#include <vector>
#include <sys/time.h>
#include <oasys/debug/Log.h>
#include <oasys/serialize/SQLSerialize.h>
#include "BundleStore.h"

/**
 * Implementation of a StorageManager with an underlying SQL
 * database.
 */
class SQLStore: public PersistentStore, public Logger  {
public:
    
    /**
     * Create a SQLStore with specfied table_name.
     * Parameter db, points to the actual implementation to which
     * different queries are forwarded
     */
    SQLStore(const char* table_name, SQLImplementation *db);
    
    /// @{

    /**
     * Close the table.
     */
    int close();
    
    /**
     *  Get an obj (identified by key) from the sql store. 
     *  @return 0 if success, -1 on error
     */
    int get(SerializableObject* obj, const int key);
 
    /**
     * Store the object with the given key.
     */
    int put(SerializableObject* obj, const int key);

    /**
     *  Put an obj in the sql store. 
     *  @return 0 if success, -1 on error
     */
    int insert(SerializableObject* obj);
    
    /**
     *  Update the object's state in the sql store.
     *  @return number updated on success, -1 on error
     */
    int update(SerializableObject* obj, const int key);
    
    /**
     *  Delete an obj (identified by key)
     *  @return 0 if success, -1 on error
     */
    int del(const int key);

    /**
     *  Return number of elements in the store.
     *  @return number of elements if success, -1 on error
     */
    int num_elements();

    /**
     *  Return list of keys for all elements in the store.
     *  @return 0 if success, -1 on error
     */
    int keys(std::vector<int> l);
    
    /**
     *  Extract all elements from the store, potentially matching the
     *  "where" clause. For each matching element, initialize the
     *  corresponding object in the vector with the values from the
     *  database.
     *
     *  @return count of extracted elements, or -1 on error
     */
    int elements(SerializableObjectVector* elements);

    /**
     * Returns the table name associated with this store
     */
    const char* table_name();

    /**
     * Checks if the table already exists.
     * @return true if table exits, false otherwise
     */
    bool has_table(const char *name);

    /**
     * Creates table a table in the store for objects which has the same type as obj. 
     * Checks if the table already exists.
     * @return 0 if success, -1 on error
     */
    int create_table(SerializableObject* obj);


    /**
     * Executes the given query. Essentially forwards the query to data_base_pointer_
     * @return 0 if success, -1 on error
     */
    int exec_query(const char* query);

    /**
     * Set's the key name. The key name is used in all functions which need
     * to create a query which refers to key. Must be initialized for proper
     * operation.
     */
    void set_key_name(const char* name);

private:

    /**
     * Name of the table in the database to which this SQLStore corresponds
     */
    const char* table_name_;    
    
    /**
     * Field name by which the objects are keyed. 
     */
    const char*  key_name_; 

    SQLImplementation*  sql_impl_;

};



#endif /* _SQL_STORE_H_ */

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

#if __MYSQL_ENABLED__

#include <oasys/debug/Debug.h>
#include <oasys/debug/Log.h>

#include "MysqlSQLImplementation.h"

namespace dtn {

MysqlSQLImplementation::MysqlSQLImplementation()
    : SQLImplementation("BLOB", "BOOLEAN"),
      Logger("/storage/mysql")
{
    query_result_ = NULL;
}

int
MysqlSQLImplementation::connect(const char* dbName)
{
    db_ = mysql_init(NULL);

    log_debug("connecting to database %s", dbName);
 
    /* connect to database */
    if (!mysql_real_connect(db_, NULL,
                            NULL, NULL, dbName, 0, NULL, 0)) {
        log_err("error connecting to database %s: %s",
                dbName, mysql_error(db_));
        return -1;
    }

    return 0;
}

const char*
MysqlSQLImplementation::get_value(int tuple_no, int field_no)
{
    const char* ret;
  
    ASSERT(query_result_);
    mysql_data_seek(query_result_, tuple_no);

    MYSQL_ROW r = mysql_fetch_row(query_result_);
    ret = r[field_no];
    return ret;
}

/*
size_t
MysqlSQLImplementation::get_value_length(int tuple_no, int field_no)
{
    unsigned long *lengths;
  
    ASSERT(query_result_);
    mysql_data_seek(query_result_, tuple_no);

    mysql_fetch_row(query_result_);
    lengths = mysql_fetch_lengths(query_result_);
    return (size_t)lengths[field_no];
}
*/

int
MysqlSQLImplementation::close()
{
    mysql_close(db_);
    db_ = NULL;
    return 0;
}

bool
MysqlSQLImplementation::has_table(const char* tablename)
{
    bool ret = false;
 
    if (query_result_ != 0) {
        mysql_free_result(query_result_);
        query_result_ = 0;
    }

    query_result_ = mysql_list_tables(db_, tablename);
    
    if (mysql_num_rows(query_result_) == 1) {
        ret = true;
    }
    
    mysql_free_result(query_result_);
    query_result_ = NULL;

    return ret;
}

int
MysqlSQLImplementation::num_tuples()
{
    int ret = -1;
    ASSERT(query_result_);
    ret = mysql_num_rows(query_result_);
    return ret;
}

int
MysqlSQLImplementation::exec_query(const char* query)
{
    int ret = -1;
    
    // free previous result state
    if (query_result_ != NULL) {
        mysql_free_result(query_result_);
        query_result_ = NULL;
    }
    
    ret = mysql_query(db_, query);
    
    if (ret == 1) {
        return ret;
    }
    
    query_result_ = mysql_store_result(db_);
    
    return ret;
}

const char* 
MysqlSQLImplementation::escape_string(const char* from) 
{
    int length = strlen(from);
    // XXX/demmer fix memory leaks
    char* to = (char *) malloc(2*length+1);
    mysql_real_escape_string(db_,to,from,length);

    return to;
}

const u_char* 
MysqlSQLImplementation::escape_binary(const u_char* from, int from_length) 
{
    int length = from_length;
    // XXX/demmer fix memory leaks
    char* to = (char *) malloc(2*length+1);
    mysql_real_escape_string(db_,to,(const char*)from,length);
    return (const u_char*) to;
}

const u_char* 
MysqlSQLImplementation::unescape_binary(const u_char* from) 
{
    return from; 
}

} // namespace dtn

#endif /* __MYSQL_ENABLED__ */

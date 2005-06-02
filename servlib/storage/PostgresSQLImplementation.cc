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

#if POSTGRES_ENABLED

#include <string.h>
#include <oasys/debug/Debug.h>
#include <oasys/util/StringBuffer.h>
#include "PostgresSQLImplementation.h"

namespace dtn {

PostgresSQLImplementation::PostgresSQLImplementation()
    : SQLImplementation("BYTEA", "BOOLEAN"),
      Logger("/storage/postgresql")
{
    query_result_ = NULL;
}

int
PostgresSQLImplementation::connect(const char* dbName)
{
    char *pghost;
    char *pgport;
    char *pgoptions;
    char *pgtty;
 
    log_debug("connecting to database %s", dbName);

    /*
     * begin, by setting the parameters for a backend connection if
     * the parameters are null, then the system will try to use
     * reasonable defaults by looking up environment variables or,
     * failing that, using hardwired constants
     */

    pghost = NULL;   	/* host name of the backend server */
    pgport = NULL;   	/* port of the backend server */
    pgoptions = NULL;   /* special options to start up the backend
                         * server */
    pgtty = NULL;   	/* debugging tty for the backend server */
    
    /**
     *  make a connection to the database 
     *
     */
    
    db_ = PQsetdb(pghost, pgport, pgoptions, pgtty, dbName);
        
    /**
     * check to see that the backend connection was successfully made
     */
    if (PQstatus(db_) == CONNECTION_BAD)
    {
        log_err("connection to database '%s' failed: %s",
                dbName, PQerrorMessage(db_));
        return -1;
    }
    
    return 0;
}

int
PostgresSQLImplementation::close()
{
    PQfinish(db_);
    return 0;
}

/*
size_t
PostgresSQLImplementation::get_value_length(int tuple_no, int field_no)
{
    size_t retval;
    const char* val = get_value(tuple_no,field_no);
    unescape_binary((const u_char*)val,&retval);
    return retval;
}
*/

const char*
PostgresSQLImplementation::get_value(int tuple_no, int field_no)
{
    const char* ret ;
    ASSERT(query_result_);
    ret = PQgetvalue(query_result_, tuple_no, field_no);
    return ret;
}

bool
PostgresSQLImplementation::has_table(const char* tablename)
{
    bool retval = 0;
    oasys::StringBuffer query;
    
    query.appendf("select * from pg_tables where tablename = '%s'", tablename);
    int ret = exec_query(query.c_str());
    ASSERT(ret == 0);
    if (num_tuples() == 1) retval  = 1;

    return retval;
}

int
PostgresSQLImplementation::num_tuples()
{
    int ret = -1;
    ASSERT(query_result_);
    ret = PQntuples(query_result_);
    return ret;
}

static int
status_to_int(ExecStatusType t)
{
    if (t == PGRES_COMMAND_OK) return 0;
    if (t == PGRES_TUPLES_OK) return 0;
    if (t == PGRES_EMPTY_QUERY) return 0;
    return -1;
}

int
PostgresSQLImplementation::exec_query(const char* query)
{
    int ret = -1;

    if (query_result_ != NULL) {
        PQclear(query_result_);
        query_result_ = NULL;
    }
    
    query_result_ = PQexec(db_, query);
    ASSERT(query_result_);
    ExecStatusType t = PQresultStatus(query_result_);
    ret = status_to_int(t);
    
    return ret;
}

const char* 
PostgresSQLImplementation::escape_string(const char* from) 
{
    int length = strlen(from);
    char* to = (char *) malloc(2*length+1);
    PQescapeString (to,from,length);
    return to;
}

const u_char* 
PostgresSQLImplementation::escape_binary(const u_char* from, int from_length) 
{
    size_t to_length;
    u_char* from1 = (u_char *) from ; 
    u_char* to =  PQescapeBytea(from1,from_length,&to_length);
    return to;
}

const u_char* 
PostgresSQLImplementation::unescape_binary(const u_char* from) 
{
    u_char* from1 = (u_char *) from ; 
    size_t to_length ;
    const u_char* to = PQunescapeBytea(from1,&to_length);
    return to;
}

} // namespace dtn

#endif /* POSTGRES_ENABLED */

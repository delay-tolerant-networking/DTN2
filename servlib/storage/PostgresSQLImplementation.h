#ifndef _POSTGRES_SQL_IMPLEMENTATION_H_
#define _POSTGRES_SQL_IMPLEMENTATION_H_

#include "SQLImplementation.h"
#include "libpq-fe.h"
#include "debug/Log.h"

/**
 * Postgres based implementation of SQL database.
 */

class PostgresSQLImplementation : public SQLImplementation, public Logger {

public:
  
    PostgresSQLImplementation(const char* dbName);
    
    // Virtual functions inherited from SQLImplementation
 
    int exec_query(const char* query);
    const char* get_value(int tuple_no, int field_no);
    int tuples();
    int close();
    bool has_table(const char* tablename);

private:
    /**
     * Used for connecting to a postgres database server 
     */
    PGconn* connect_db(const char* dbName);
    
    PGconn* data_base_pointer_; /// pointer to the postgres connection
    PGresult* query_result_;    /// result of the last query performed
 
};

#endif /* _POSTGRES_SQL_IMPLEMENTATION_H_ */

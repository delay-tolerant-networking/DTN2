#ifndef _MYSQL_SQL_IMPLEMENTATION_H_
#define _MYSQL_SQL_IMPLEMENTATION_H_

#include "SQLImplementation.h"
#include "debug/Log.h"
#include "mysql.h"




/**
 * Mysql based implementation of SQL database.
 */

class MysqlSQLImplementation : public SQLImplementation, public Logger {

public:

    MysqlSQLImplementation(const char* dbName);

    // Virtual functions inherited from SQLImplementation
    
    int exec_query(const char* query);
    const char* get_value(int tuple_no, int field_no);
    int tuples();
    int close();
    bool has_table(const char* tablename);

private:
    /**
     * Used for connecting to a postgres database server 
     **/
    MYSQL* connect_db(const char* dbName);
    
    MYSQL* data_base_pointer_;  /// pointer to the postgres connection
    MYSQL_RES* query_result_;  /// result of the last query performed
};

#endif /* _MYSQL_SQL_IMPLEMENTATION_H_ */

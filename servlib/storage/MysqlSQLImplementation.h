#ifndef _MYSQL_SQL_IMPLEMENTATION_H_
#define _MYSQL_SQL_IMPLEMENTATION_H_

#include "SQLImplementation.h"
#include "debug/Log.h"
#include "mysql.h"
#include <string>

class MysqlSQLImplementation : public SQLImplementation, public Logger {
public:
    MysqlSQLImplementation(const char* dbName);
    int exec_query(const char* query);
    const char* get_value(int tuple_no, int field_no);
    int tuples();
    int close();
    bool has_table(const char* tablename);

private:
    MYSQL* connect_db(const char* dbName);
    
    MYSQL* data_base_pointer_;
    MYSQL_RES* query_result_;
};

#endif /* _MYSQL_SQL_IMPLEMENTATION_H_ */

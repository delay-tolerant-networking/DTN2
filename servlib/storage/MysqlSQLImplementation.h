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
    MysqlSQLImplementation();

    ///@{
    /// Virtual functions inherited from SQLImplementation
    int connect(const char* dbname);
    int close();
    bool has_table(const char* tablename);
    int exec_query(const char* query);
    int num_tuples();
    const char* get_value(int tuple_no, int field_no);
    ///@}

private:
    MYSQL* db_;  		///< the db connection
    MYSQL_RES* query_result_;	/// result of the last query performed
};

#endif /* _MYSQL_SQL_IMPLEMENTATION_H_ */

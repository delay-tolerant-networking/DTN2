
#include "debug/Debug.h"
#include "MysqlSQLImplementation.h"

MysqlSQLImplementation::MysqlSQLImplementation(const char *dbName)
    : Logger("/storage/mysql")
{
    MYSQL *db;
    db = connect_db(dbName);
    data_base_pointer_ = db;
    query_result_ = NULL;
}

MYSQL*
MysqlSQLImplementation::connect_db(const char* dbName)
{
    MYSQL *conn;

    conn = mysql_init(NULL);

    log_debug("connecting to database %s", dbName);
 
    /* Connect to database */
    if (!mysql_real_connect(conn, NULL,
                            NULL, NULL, dbName, 0, NULL, 0)) {
        log_err("error connecting to database %s: %s",
                dbName, mysql_error(conn));
        exit(1);
    }

    return conn;
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

int
MysqlSQLImplementation::close()
{
    mysql_close(data_base_pointer_);
    return 0;
}

bool
MysqlSQLImplementation::has_table(const char* tablename)
{
    bool retval = 0;
 
    if (query_result_ != 0) {
        mysql_free_result(query_result_);
        query_result_ = 0;
    }

    query_result_ = mysql_list_tables(data_base_pointer_, tablename);
    
    if (mysql_num_rows(query_result_) == 1)
        retval = 1;
    mysql_free_result(query_result_);
    query_result_ = NULL;

    return retval;
}

int
MysqlSQLImplementation::tuples()
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
    ret = mysql_query(data_base_pointer_,query);
    if (ret == 1) return ret ; 
    query_result_ = mysql_store_result(data_base_pointer_);
    
    return ret;
}


#include "debug/Debug.h"
#include "MysqlSQLImplementation.h"

MysqlSQLImplementation::MysqlSQLImplementation()
    : Logger("/storage/mysql")
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

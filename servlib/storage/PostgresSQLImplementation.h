#ifndef _POSTGRES_SQL_IMPLEMENTATION_H_
#define _POSTGRES_SQL_IMPLEMENTATION_H_

#include "SQLImplementation.h"
#include "libpq-fe.h"
#include "debug/Log.h"

class PostgresSQLImplementation : public SQLImplementation, public Logger {

public:
    PostgresSQLImplementation(const char* dbName);
    int exec_query(const char* query);
    const char* get_value(int tuple_no, int field_no);
    int tuples();
    int close();
    bool has_table(const char* tablename);

private:
    PGconn* connect_db(const char* dbName);
    
    PGconn* data_base_pointer_; 
    PGresult* query_result_;
    int query_called_;
};

#endif /* _POSTGRES_SQL_IMPLEMENTATION_H_ */

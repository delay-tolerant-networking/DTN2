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
    PostgresSQLImplementation();
    
    ///@{
    /// Virtual functions inherited from SQLImplementation
    int connect(const char* dbname);
    int close();
    bool has_table(const char* tablename);
    int exec_query(const char* query);
    int num_tuples();
    const char* get_value(int tuple_no, int field_no);
//    size_t get_value_length(int tuple_no, int field_no);
    

    const char* escape_string(const char* from);
    const u_char* escape_binary(const u_char* from, int from_length);
    const u_char* unescape_binary(const u_char* from);
  
    const char* binary_datatype();

    ///@}

private:
    PGconn* db_;  		///< the db connection
    PGresult* query_result_;	/// result of the last query performed
};

#endif /* _POSTGRES_SQL_IMPLEMENTATION_H_ */

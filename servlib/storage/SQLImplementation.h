#ifndef _SQL_IMPLEMENTATION_H_
#define _SQL_IMPLEMENTATION_H_


#include <string.h>
#include "Serialize.h"


/**
 * Interface required for a a particular SQL database.
 */
class SQLImplementation {
public:

    /**
     * Open a connection to the database.
     */
    virtual int connect(const char* dbname) = 0;

    /**
     * Close the connection to the database.
     */
    virtual int close() = 0;

    /**
     * Check if the database has the specified table.
     */
    virtual bool has_table(const char* tablename) = 0;

    /**
     * Run the given sql query.
     */

    virtual int exec_query(const char* query) = 0;

    
    /**
     * Return the count of the tuples in the previous query.
     */
    virtual int num_tuples() = 0;
    
    /**
     * Get the value at the given tuple / field for the previous query.
     * Its an error to call this function before calling exec_query
     */

    virtual const char* get_value(int tuple_no, int field_no) = 0;


    /**
     * Return the length  a particular element in the answers set 
     * of the executed query. Important for handling binary data.
     * Its an error to call this function before calling exec_query
     * Not supported for now. No common way to use this in both
     * postgres and mysql.
    */
    // virtual size_t get_value_length(int tuple_no, int field_no) = 0;

    
    
    /**
     * Query construction related functions. This is where, we 
     * address the differences between different databases interfaces. 
     */


    /**
     * Returns escaped version of a normal string to be used in SQL query.
     *
     */
    virtual const char* escape_string(const char* from) =0;

    /**
     * Returns escaped version of a binary buffer to be used in SQL query.
     *
     */
    virtual  const u_char* escape_binary(const u_char* from, int from_length) =0;

    /**
     * Unescapes the retured value of a SQL query to a binary buffer. 
     */
    virtual const u_char* unescape_binary(const u_char* from) =0;

    
    /**
     * Returns name of binary data type. Used for specifying
     * data type of the columnn for storing binary data when
     * creating table
     */
    virtual const char* binary_datatype() =0;


};

#endif /* _SQL_IMPLEMENTATION_H_ */

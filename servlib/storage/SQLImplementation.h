#ifndef _SQL_IMPLEMENTATION_H_
#define _SQL_IMPLEMENTATION_H_

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
     */
    virtual const char* get_value(int tuple_no, int field_no) = 0;
};

#endif /* _SQL_IMPLEMENTATION_H_ */

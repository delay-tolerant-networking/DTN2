#ifndef _SQL_IMPLEMENTATION_H_
#define _SQL_IMPLEMENTATION_H_


/**
 * Interface required for a a particular SQL database.
 */
class SQLImplementation {
public:
    virtual bool has_table(const char* tablename) = 0;  
    virtual int exec_query(const char* query) = 0;
    virtual const char* get_value(int tuple_no, int field_no) = 0;
    virtual int tuples() = 0;
    virtual int close() =0;
};

#endif /* _SQL_IMPLEMENTATION_H_ */

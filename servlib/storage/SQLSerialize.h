#ifndef _SQL_SERIALIZE_H_
#define _SQL_SERIALIZE_H_

/**
 * This file defines the serialization and deserialization objects to
 * be used in an SQL storage context.
 */

#include "Serialize.h"
#include "util/StringBuffer.h"

class SQLImplementation;

/**
 * SQLQuery implements common functionality used when building up a
 * SQL query string.
 */
class SQLQuery : public SerializeAction {
public:
    /**
     * Constructor.
     */
    SQLQuery(action_t type, const char* initial_query = 0);

    /**
     * Return the constructed query string.
     */
    const char* query();
    
protected:
    StringBuffer query_;
};

/**
 * SQLInsert is a SerializeAction that builts up a SQL "INSERT INTO"
 * query statement based on the values in an object.
 */
class SQLInsert : public SQLQuery {
public:
    /**
     * Constructor.
     */
    SQLInsert();
  
    /**
     * Since insert doesn't modify the object, define a variant of
     * action() that operates on a const SerializableObject.
     */
    int action(const SerializableObject* const_object)
    {
        return(action((SerializableObject*)const_object));
    }
        
    // Virtual functions inherited from SerializeAction
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, int32_t* i);
    void process(const char* name, int16_t* i);
    void process(const char* name, int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, size_t len);
    void process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy);
    void process(const char* name, std::string* s);
};

/**
 * SQLTableFormat is a SerializeAction that builts up a SQL "CREATE
 * TABLE" query statement based on the values in an object.
 */
class SQLTableFormat : public SQLQuery {
public:
    /**
     * Constructor.
     */
    SQLTableFormat();

    /**
     * Since table format doesn't modify the object, define a variant
     * of action() that operates on a const SerializableObject.
     */
    int action(const SerializableObject* const_object)
    {
        return(action((SerializableObject*)const_object));
    }
        
    // Virtual functions inherited from SerializeAction
    void process(const char* name,  SerializableObject* object);
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, size_t len);
    void process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy);
    void process(const char* name, std::string* s);

 protected:
    void append(const char* name, const char* type);
    StringBuffer column_prefix_;
};

/**
 * SQLExtract is a SerializeAction that constructs an object's
 * internals from the results of a SQL "select" statement.
 */
class SQLExtract : public SerializeAction {
public:
    SQLExtract(SQLImplementation *db);

    // get the next field from the db
    const char* next_field() ;

    // Virtual functions inherited from SerializeAction
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, size_t len);
    void process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy);
    void process(const char* name, std::string* s); 

 protected:
    int field_;		///< counter over the fields in the returned tuple
    
 private:
    SQLImplementation *db_;
};

#endif /* _SQL_SERIALIZE_H_ */

#ifndef _SQL_SERIALIZE_H_
#define _SQL_SERIALIZE_H_

/**
 * This file defines the serialization and deserialization objects to
 * be used in an SQL storage context.
 */

#include "Serialize.h"

/**
 * SQLInsert is a SerializeAction that builts up a SQL "INSERT INTO"
 * query statement based on the values in an object.
 */
class SQLInsert : public SerializeAction {
public:
    /**
     * Constructor.
     */
    SQLInsert(const char* table)
        : SerializeAction(MARSHAL)
    {
        // snprintf(query_, querylen_, "INSERT INTO %s", table);
    }

    /**
     * Return the constructed query string.
     */
    const char* query() { return query_; }

    /**
     * The virtual process function to do the serialization.
     */
    int action(SerializableObject* object);
    
    /**
     * Since the insert operation doesn't actually modify the
     * SerializableObject, define a variant of process() that allows a
     * const SerializableObject* as the object parameter.
     */
    int process(const SerializableObject* object)
    {
        return process((SerializableObject*)object);
    }

    // Virtual functions inherited from SerializeAction
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, size_t len);
    void process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy);
    void process(const char* name, std::string* s);

protected:
    char query_[256]; // XXX this needs to be growable, use a class
};

/**
 * SQLExtract is a SerializeAction that constructs an object's
 * internals from the results of a SQL "select" statement.
 */
class SQLExtract : public SerializeAction {
public:
    // Constructor
    /// XXX need some parameter representing the result
    SQLExtract()
        : SerializeAction(UNMARSHAL)
    {
    }

    int process(SerializableObject* object);

    // Virtual functions inherited from SerializeAction
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, size_t len);
    void process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy);
    void process(const char* name, std::string* s); 
};

#endif /* _SQL_SERIALIZE_H_ */

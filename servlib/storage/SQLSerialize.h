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

    SQLInsert();
  

    /**
     * Return the constructed query string.
     */
    const char* query() { return query_.c_str(); }

    /**
     * The virtual process function to do the serialization.
     */
    int action(SerializableObject* object);
    
    //    int process(const char* name, const SerializableObject* object);

    /**
     * Since the insert operation doesn't actually modify the
     * SerializableObject, define a variant of process() that allows a
     * const SerializableObject* as the object parameter.
     */
    int process(const char* name,const SerializableObject* object)
    {
        return process(name,(SerializableObject*)object);
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
    std::string query_;
    
 private:
    int firstmember_;
    
    void 
      SQLInsert::append_query(const char* s) ;
};



/**
 * SQLTableFormat is a SerializeAction that builts up a SQL "CREATE TABLE"
 * query statement based on the values in an object.
 */
class SQLTableFormat : public SerializeAction {
public:
    /**
     * Constructor.
     */

  SQLTableFormat();


    /**
     * Return the constructed query string.
     */
    const char* query() { return query_.c_str(); }

    /**
     * The virtual process function to do the serialization.
     */
    int action(SerializableObject* object);
    
    virtual void process(const char* name,  SerializableObject* object) ;
  
    /**
     * Since this operation doesn't actually modify the
     * SerializableObject, define a variant of process() that allows a
     * const SerializableObject* as the object parameter.
     */
    void process(const char* name,const SerializableObject* object)
    {
        return process(name,(SerializableObject*)object);
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
    std::string query_;
    std::string column_prefix_ ;
    
 private:
    int firstmember_;
    
    void 
      SQLTableFormat::append_query(const char* name, std::string s) ;

};




/**
 * SQLExtract is a SerializeAction that constructs an object's
 * internals from the results of a SQL "select" statement.
 */
class SQLExtract : public SerializeAction {
public:

  SQLExtract();

    /**
     * The virtual process function to do the unserialization.
     */
    int action(SerializableObject* object);

    virtual const char* next_slice() = 0;



    // Virtual functions inherited from SerializeAction
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, size_t len);
    void process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy);
    void process(const char* name, std::string* s); 

 protected:
    size_t  field_;		///< counter over the no. of fields in the returned tuple
};


#endif /* _SQL_SERIALIZE_H_ */

#ifndef _SQL_STORE_H_
#define _SQL_STORE_H_

#include <sys/time.h>
#include "BundleStore.h"
#include "SQLSerialize.h"


/**
 * Implementation of a StorageManager with an underlying SQL
 * database.
 */
class SQLStore  {
public:
    SQLStore(const char* table_name, SQLImplementation *db);
    
    /// @{ 


    int get(SerializableObject* obj, const int key);
 
    int put(SerializableObject* obj);
    int del(const int key);
    int num_elements();
    void keys(std::vector<int> l);
    void elements(std::vector<SerializableObject*> l);
      

    /// @}

protected:

    
    const char* table_name(); 
    int create_table(SerializableObject* obj);
    int exec_query(const char* query);
    void set_key_name(const char* name);

    friend class SQLBundleStore;

private:
    const char* table_name_;
   
    // Name of the field by which the objects are keyed. 
    const char*  key_name_;

    SQLImplementation* data_base_pointer_;
};



#endif /* _SQL_STORE_H_ */

#ifndef _POSTGRES_SQL_STORE_H_
#define _POSTGRES_SQL_STORE_H_

#include "SQLStore.h"

#define HAVE_NAMESPACE_STD
#include "libpq++.h"
#

class PostgresSQLStore : public SQLStore {

public:
    PostgresSQLStore(const char* name, SerializableObject* obj, PgDatabase*  db);

    int get(SerializableObject* obj, const int key);
    int num_elements();
    void keys(std::vector<int> l);
    


 protected: 
    int  exec_query(const char* query) ; 

 private:
  PgDatabase*  data_base_pointer_ ; 

};


#endif /* _POSTGRES_SQL_STORE_H_ */

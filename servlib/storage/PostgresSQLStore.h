#include "SQLStore.h"
#include "libpq++.h"

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

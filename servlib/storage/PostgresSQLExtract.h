#include "Serialize.h"
#include "SQLSerialize.h"

#include <libpq++.h>


class PostgresSQLExtract: public SQLExtract {

 public:
  PostgresSQLExtract( PgDatabase*  db) ;
  
 protected:
  virtual  const char* next_slice();
  
 
 private:
  PgDatabase*  data_base_pointer_ ; 
};


#include "PostgresSQLStore.h"
#include "PostgresSQLExtract.h"
#include "iostream"

using namespace std;

/**
 * Constructor.
 */

PostgresSQLStore::PostgresSQLStore(const char* name, SerializableObject* obj, PgDatabase* db)
  : SQLStore(name,obj)
{
  data_base_pointer_ = db;


  // Create table (if its already there then nothing happens
  create_table(obj);


}


int
PostgresSQLStore::exec_query(const char* query) 
{    
  int ret = data_base_pointer_->Exec(query);
  return ret;
}


int 
PostgresSQLStore::get(SerializableObject* obj, const int key) 
{

  int status = exec_query(get_sqlquery(obj,key));
  if ( status == 1) {
    cout <<  "query done properly \n";
  }
 else {
   cout <<  "query  error  \n";
   return -1;
 }
  
  // use SQLExtract to fill obj
 PostgresSQLExtract xt (data_base_pointer_) ;
 xt.action(obj);
 return 0;
}


int 
PostgresSQLStore::num_elements()
{

  int status = exec_query(num_elements_sqlquery());
  if ( status == 1) {
    cout <<  "query done properly \n";
  }
  else {
    cout <<  "query  error  \n";
    return -1;
  }
  const char* answer = data_base_pointer_->GetValue(0,0);
  if (answer == NULL) return 0;
  return atoi(answer);
}


void 
PostgresSQLStore::keys(std::vector<int> l) {

 int status = exec_query(keys_sqlquery());
  if ( status == 1) {
    cout <<  "query done properly \n";
  }
  else {
    cout <<  "query  error  \n";
    return ;
  }

  int n = data_base_pointer_->Tuples();
  for(int i=0;i<n;i++) {
    // ith element is set to 
    const char* answer = data_base_pointer_->GetValue(i,0);
    int answer_int =  atoi(answer);
    l[i]=answer_int;
  }
}

#include "SQLStore.h"

#include "iostream"

using namespace std;


/**
 * Constructor.
 */

SQLStore::SQLStore(const char* name, SerializableObject* obj)
  : PersistentStore()
{
  table_name_ = name;
  OBJECT_ID_FIELD = "oid" ;
    
}


string int_to_string(const int i) {
  char buffer [50];
  sprintf(buffer,"%u",i);
  string key_string(buffer) ;
  return key_string;
}


int
SQLStore::create_table(SerializableObject* obj) {

  // create sql string that corresponds to table creation
  SQLTableFormat t;
  t.action(obj);
  string table_create = "" ;
  string obj_id_field = "";
  obj_id_field = obj_id_field + OBJECT_ID_FIELD + " BIGINT, " ;
  table_create = table_create +  " CREATE TABLE " + table_name_ + " (" + obj_id_field +  t.query() + ");" ;
  
  
  // actually execute this query
  int retval = 0;
  retval = exec_query(table_create.c_str());
  if (retval == 0) {
    cout << " table exists " << endl ; 
  }
  else {
    cout << " table created " << endl; 
  }
  return retval;


}

const char*
SQLStore::get_sqlquery(SerializableObject* obj, const int key){
  
 string query = "";
 query = query +  " SELECT * FROM  " + table_name_ ;
 query = query + " where " + OBJECT_ID_FIELD  + " = " + int_to_string(key) + " ; "  ; 
 

 return  query.c_str() ;
}

     
int 
SQLStore::put(SerializableObject* obj, const int key){


  SQLInsert s ;
  s.action(obj);
  string insert = "";
  insert =  insert  +  " INSERT INTO "  + table_name_  + " VALUES (" + int_to_string(key) + "," + insert + ");" ;

  int retval = exec_query(insert.c_str());
  return retval;
}


     
int 
SQLStore::del(const int key){
  string del = "";
  del =  del +   " DELETE  FROM  "  + table_name_  + " WHERE " + OBJECT_ID_FIELD  + " = " + int_to_string(key) + " ; "  ; 

  int retval = exec_query(del.c_str());
  return retval;
 
}
     
const char*
SQLStore::num_elements_sqlquery(){

 string query = "";
 query = query +  " SELECT  count(*) FROM  " + table_name_  + " ; " ;
 return  query.c_str() ;

}
     

const char*
SQLStore::keys_sqlquery(){

 string query = "";
 query = query +  " SELECT  " + OBJECT_ID_FIELD + "  FROM  " + table_name_  + " ; " ;
 return  query.c_str() ;


}
     

void 
SQLStore::elements(std::vector<SerializableObject*> l) {

  int n = num_elements();
  vector<int> vec_ids(n) ;
    keys(vec_ids);
  for(int i =0;i<n;i++) {
    get(l[i],vec_ids[i]);
  }

}


#include "SQLStore.h"
#include "SQLImplementation.h"

#include <iostream>

using namespace std;


/**
 * Constructor.
 */

SQLStore::SQLStore(const char* name, SerializableObject* obj, SQLImplementation* db)
    : PersistentStore()
{
    data_base_pointer_ = db;
    table_name_ = name;
    SQLStore::OBJECT_ID_FIELD = "dtnoid" ;

    // Create table (if its already there then nothing happens

    if (db->has_table(table_name_) == 0)
        create_table(obj);
    else {
        cout << " take it easy table was already there " << endl ; 
    }
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
    table_create = table_create +  " CREATE TABLE " + table_name_ + " (" + obj_id_field +  t.query() + ")" ;
  
  
    // actually execute this query
    int retval = 0;
    retval = exec_query(table_create.c_str());
    if (retval == -1) {
        cout << " table exists, but i shud not ...ok i will exit " << endl ; 
        exit(0);
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
    query = query + " where " + OBJECT_ID_FIELD  + " = " + int_to_string(key) + "  "  ; 
 

    return  query.c_str() ;
}

     
int 
SQLStore::put(SerializableObject* obj, const int key){


    SQLInsert s ;
    s.action(obj);
    string insert = "";
    insert =  insert  +  " INSERT INTO "  + table_name_  + " VALUES (" + int_to_string(key) + "," + s.query() + ")" ;

    int retval = exec_query(insert.c_str());
    return retval;
}


     
int 
SQLStore::del(const int key){
    string del = "";
    del =  del +   " DELETE  FROM  "  + table_name_  + " WHERE " + OBJECT_ID_FIELD  + " = " + int_to_string(key) + "  "  ; 

    int retval = exec_query(del.c_str());
    return retval;
 
}
     
const char*
SQLStore::num_elements_sqlquery(){

    string query = "";
    query = query +  " SELECT  count(*) FROM  " + table_name_  + "  " ;
    return  query.c_str() ;

}
     

     
const char*
SQLStore::table_name(){

    return table_name_ ; 

}
     

const char*
SQLStore::keys_sqlquery(){

    string query = "";
    query = query +  " SELECT  " + OBJECT_ID_FIELD + "  FROM  " + table_name_  + "  " ;
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



// more below, earlier they were in implementation specific stuff
// don't really need this thingie..

int
SQLStore::exec_query(const char* query) 
{    
    int ret = data_base_pointer_->exec_query(query);
    return ret;
}


int 
SQLStore::get(SerializableObject* obj, const int key) 
{

  
    int status = exec_query(get_sqlquery(obj,key));
    if ( status == 0) {
        cout <<  "query done properly ...get type\n";
    }
    else {
        cout <<  "query  error  \n";
        return -1;
    }
  
    // use SQLExtract to fill obj
    SQLExtract xt (data_base_pointer_) ;
    xt.action(obj);
    return 0;
}


int 
SQLStore::num_elements()
{

    int status = exec_query(num_elements_sqlquery());
    if ( status == 0) {
        cout <<  "query done properly \n";
    }
    else {
        cout <<  "query  error  \n";
        return -1;
    }
    const char* answer = data_base_pointer_->get_value(0,0);
    if (answer == NULL) return 0;
    return atoi(answer);
}


void 
SQLStore::keys(std::vector<int> l) {

    int status = exec_query(keys_sqlquery());
    if ( status == 0) {
        cout <<  "query done properly \n";
    }
    else {
        cout <<  "query  error  \n";
        return ;
    }

    int n = data_base_pointer_->tuples();
    for(int i=0;i<n;i++) {
        // ith element is set to 
        const char* answer = data_base_pointer_->get_value(i,0);
        int answer_int =  atoi(answer);
        l[i]=answer_int;
    }
}

/******************************************************************************
 *
 * SQLBundleStore
 *
 *****************************************************************************/


int 
SQLBundleStore::delete_expired(const time_t now) {


    string del = "";
    const char* field = "expiration";
    del =  del +   " DELETE  FROM  "  + store_->table_name() + " WHERE " + field  + " > " + int_to_string(now) + "  "  ; 

    int retval = 1;
    retval = store_->exec_query(del.c_str());
    return retval;
 
}
     


bool
SQLBundleStore::is_custodian(int bundle_id) {

    exit(-1);
    return 0;

}

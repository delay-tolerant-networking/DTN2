
#include "SQLStore.h"
#include "SQLImplementation.h"

#include <iostream>
#include "bundling/Bundle.h"

using namespace std;


/**
 * Constructor.
 */

SQLStore::SQLStore(const char* name, const char* id_field, SerializableObject* obj, SQLImplementation* db)
    : PersistentStore()
{
    cout << " trying to initialize sql store ... " ; 
    data_base_pointer_ = db;
    table_name_ = name;
    object_id_field_ =  id_field;

    // Create table (if its already there then nothing happens
    
    cout << " trying to create table .." ; 
    if (db->has_table(table_name_) == 0) {
        create_table(obj);
        cout << " created table  ..\n" ; 
    }
    else {
        cout << " Table with name  " << table_name_ << " exists " << endl ; 
    }
}

int 
SQLStore::get(SerializableObject* obj, const int key) 
{
    StringBuffer query ;
    query.appendf("SELECT * FROM %s where %s = %d",table_name_,object_id_field_,key);

    cout << "  query is " << query.c_str() << endl ; 
    int status = exec_query(query.c_str());
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
SQLStore::put(SerializableObject* obj, const int key){

    
    SQLInsert s(table_name_); ;
    s.action(obj);
    const char* insert_str = s.query();
    int retval = exec_query(insert_str);
    return retval;
}


     
int 
SQLStore::del(const int key)
{
    StringBuffer query ;
    query.appendf("DELETE FROM %s where %s = %d",table_name_,object_id_field_,key);
    int retval = exec_query(query.c_str());
    return retval;
}



int 
SQLStore::num_elements()
{
    StringBuffer query;
    query.appendf(" SELECT  count(*) FROM  %s ",table_name_);
    int status = exec_query(query.c_str());
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
SQLStore::keys(std::vector<int> l) 
{
    StringBuffer query ;
    query.appendf("SELECT %s FROM %s ",object_id_field_,table_name_);
    int status = exec_query(query.c_str());
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

void 
SQLStore::elements(std::vector<SerializableObject*> l) 
{
    int n = num_elements();
    vector<int> vec_ids(n) ;
    keys(vec_ids);
    for(int i =0;i<n;i++) {
        get(l[i],vec_ids[i]);
    }

}

// Protected functions

int
SQLStore::create_table(SerializableObject* obj) 
{
    
    // create sql string that corresponds to table creation
    SQLTableFormat t(table_name_);

    t.action(obj);

    int retval = exec_query(t.query());
    return retval;
}

const char*
SQLStore::table_name()
{
    return table_name_ ; 
}
 
int
SQLStore::exec_query(const char* query) 
{    
    int ret = data_base_pointer_->exec_query(query);
    return ret;
}

/******************************************************************************
 *
 * SQLBundleStore
 *
 *****************************************************************************/


SQLBundleStore::SQLBundleStore(const char* table_name, SQLImplementation* db)
    : BundleStore()
{
    PersistentStore *store ;
//    cout << "hello u "; 
    Bundle tmpobj;
    
     
    store = new SQLStore(table_name,"bundleid",&tmpobj,db);
    //  cout << "hello u ";
    // XXX fixme
//    this->BundleStore::init(store);
}
int 
SQLBundleStore::delete_expired(const time_t now) 
{
    const char* field = "expiration";
    StringBuffer query ;
    query.appendf("DELETE FROM %s  WHERE  %s > %lu",store_->table_name(),field,now);
    
    int retval = store_->exec_query(query.c_str());
    return retval;
}
     


bool
SQLBundleStore::is_custodian(int bundle_id) 
{
    exit(-1);
    return 0;

}

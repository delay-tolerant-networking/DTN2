
#include "SQLStore.h"
#include "SQLImplementation.h"

#include <iostream>
#include "bundling/Bundle.h"

using namespace std;


/**
 * Constructor.
 */

SQLStore::SQLStore(const char* table_name, SQLImplementation* db)
{
    cout << " trying to initialize sql store ... " ; 
    data_base_pointer_ = db;
    table_name_ = table_name;
}

int 
SQLStore::get(SerializableObject* obj, const int key) 
{
    StringBuffer query ;
    query.appendf("SELECT * FROM %s where %s = %d",table_name_,key_name_,key);

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
SQLStore::put(SerializableObject* obj){

    
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
    query.appendf("DELETE FROM %s where %s = %d",table_name_,key_name_,key);
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
    query.appendf("SELECT %s FROM %s ",key_name_,table_name_);
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

    if (data_base_pointer_->has_table(table_name_)) {
	cout << " Table with name  " << table_name_ << " exists " << endl ; 
	return 0;
    }
    cout << " trying to create table .." ; 
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

void
SQLStore::set_key_name(const char* name) 
{    
    key_name_ = name;
}

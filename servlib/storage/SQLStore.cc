
#include "SQLStore.h"
#include "SQLImplementation.h"
#include "bundling/Bundle.h"

/**
 * Constructor.
 */

SQLStore::SQLStore(const char* table_name, SQLImplementation* db)
     : Logger("/storage/sqlstore")
{

    data_base_pointer_ = db;
    table_name_ = table_name;
}

int 
SQLStore::get(SerializableObject* obj, const int key) 
{
    
    
    ASSERT(key_name_); //key_name_ must be initialized 

    StringBuffer query ;
    query.appendf("SELECT * FROM %s where %s = %d",table_name_,key_name_,key);

    int status = exec_query(query.c_str());
    if ( status != 0)  return status ; 
    
    SQLExtract xt (data_base_pointer_) ;     
    xt.action(obj); // use SQLExtract to fill the object
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

    ASSERT(key_name_); //key_name_ must be initialized 
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
    if ( status != 0) return -1;

    const char* answer = data_base_pointer_->get_value(0,0);
    if (answer == NULL) return 0;
    ASSERT(answer >= 0);
    return atoi(answer);
}


int
SQLStore::keys(std::vector<int> l) 
{
    ASSERT(key_name_); //key_name_ must be initialized 
    StringBuffer query ;
    query.appendf("SELECT %s FROM %s ", key_name_, table_name_);
    int status = exec_query(query.c_str());
    if ( status != 0) return status;
    
    int n = data_base_pointer_->num_tuples();
    if (n < 0) return -1;

    for(int i=0;i<n;i++) {
        // ith element is set to 
        const char* answer = data_base_pointer_->get_value(i,0);
        int answer_int =  atoi(answer);
        l[i]=answer_int;
    }
    return 0;
}

int 
SQLStore::elements(SerializableObjectVector* elements) 
{
    StringBuffer query;
    query.appendf("SELECT * from %s", table_name_);

    int status = exec_query(query.c_str());
    if (status != 0) {
        return status;
    }

    size_t n = data_base_pointer_->num_tuples();
    if (n < 0) {
        log_err("internal database error in elements()");
        return -1;
    }

    if (n > elements->size()) {
        log_err("element count %d greater than vector %d", n, elements->size());
        return -1;
    }

    SQLExtract extract(data_base_pointer_);
    SerializableObjectVector::iterator iter = elements->begin();
    for (size_t i = 0; i < n; i++) {
        extract.action(*iter);
        ++iter;
    }

    return n;
}

/******************************************************************************
 *
 * Protected functions
 *
 *****************************************************************************/



bool 
SQLStore::has_table(const char* name) {
    
    log_debug("checking for existence of table '%s'", name);
    bool retval =  data_base_pointer_->has_table(name);
    
    if (retval) 
	log_debug("table with name '%s' exists", name);
    else
    	log_debug("table with name '%s' does not exist", name);
    return retval; 
}

int
SQLStore::create_table(SerializableObject* obj) 
{
    if (has_table(table_name_)) {
	return 0;
    }
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
    log_debug("executing query '%s'", query);
    int ret = data_base_pointer_->exec_query(query);
    log_debug("query result status %d", ret);

    return ret;
}

void
SQLStore::set_key_name(const char* name) 
{    
    key_name_ = name;
}

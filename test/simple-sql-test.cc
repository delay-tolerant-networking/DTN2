
#include <stdio.h>
#include "debug/Log.h"
#include "bundling/Bundle.h"
#include "storage/SQLSerialize.h"
#include "storage/PostgresSQLExtract.h"

#include <libpq++.h>
#include "iostream"
using namespace std;


class foo : public SerializableObject {
public:
  foo() {} ;
  virtual ~foo() {} ; 
  u_int16_t id ;
  u_int32_t  f1;
  u_int8_t f2 ; 
  bool b1;
  std::string  s1;
  void serialize(SerializeAction* a) ;

  void print() ; 
};

void
foo::print() {

  cout << " this object is  " << f1 << "  char is---" << f2 << "--- id is  " << id <<  endl ; 

}

void
foo::serialize(SerializeAction* a)
{
  a->process("id", &id);
  a->process("f1", &f1);
  a->process("f2", &f2);
  
}


void 
playsql() {

  //  foo o1; o1.id = 771 ; o1.f1 = 123; o1.f2 = 'a'; foo o2;
  
  Bundle o1, o2;  o1.bundleid_ = 11;
  o1.source_.set_tuple("bundles://internet/tcp://foo");

  //  cout << " read stuff \n";
  // cin >> o1.id  >> o1.f1 >> o1.f2 ;
  // cout << "  read end  \n\n" ; 

  const char* table_name = "try";

  SQLInsert s ;
  s.action(&o1);
  string insert = s.query();
  string dummy = "";
  insert =  dummy + " INSERT INTO "  + table_name  + " VALUES (" + insert + ");" ;

  SQLTableFormat t;

  t.action(&o1);
  string table_create = t.query();
  table_create = dummy + " CREATE TABLE " + table_name + " (" + table_create + ");" ;

  string query = dummy + " select * from " + table_name + " ; " ; 

  cout << " notice below \n\n" ; 
  cout << insert << endl;
  cout << table_create << endl ; 
  cout << query << endl ; 
  cout << " notice above \n\n" ; 
  
  PgDatabase data("dbname=cse544");
  if ( data.ConnectionBad() )
    {
      cout << "Connection was unsuccessful..." << endl \
	   << "Error message returned: " << data.ErrorMessage() << endl;
      exit (0);
    }
  else
    cout << "Connection successful...  will try  queries below:" << endl;
  
  //  int PgConnection::ExecCommandOk(const char *query)

  int status ;


  if ( (status = data.ExecCommandOk( table_create.c_str() )) == 1) {
    cout <<  "table creation done properly \n";
  }
  else {
    cout <<  "table creation  error  \n";

  }


  if ( (status = data.ExecCommandOk( insert.c_str() )) == 1) {
    cout <<  "insertion done properly \n";
  }
  else {
    cout <<  "insertion  error  \n";

  }


  if ( (status = data.ExecTuplesOk( query.c_str() )) == 1) {
    cout <<  "query done properly \n";
  }
  else {
    cout <<  "query  error  \n";

  }
  // exectute query and return results into o2
  PostgresSQLExtract xt (&data) ;
  xt.action(&o2);
    
 
  cout << " printing both objects \n";
  

    SQLInsert s2  ;
  s2.action(&o1);
  string insert2 = s2.query();

  SQLInsert s3;

  s3.action(&o2);
  string insert3 = s3.query();

  cout << " finally the two objects are \n " << insert2 << endl << insert3 << endl ; 

  exit(0);

}  

int
main(int argc, const char** argv)
{


    Log::init();
    playsql();
    Bundle b, b2;
    b.bundleid_ = 100;
    ASSERT(!b.source_.valid());
    ASSERT(!b2.source_.valid());
    
    b.source_.set_tuple("bundles://internet/tcp://foo");
    ASSERT(b.source_.valid());
    ASSERT(b.source_.region().compare("internet") == 0);
    ASSERT(b.source_.admin().compare("tcp://foo") == 0);


    SQLInsert s ;

    logf("/test", LOG_INFO, "trying stuff   sqlinsert");
    if (s.action(&b) != 0) {
        logf("/test", LOG_ERR, "error in sql insert");
        exit(1);
    }
    const char* x ;
    x = s.query();

    //
    
    SQLTableFormat tf;

    logf("/test", LOG_INFO, "trying stuff  table format  ");
    if (tf.action(&b) != 0) {
        logf("/test", LOG_ERR, "error in table format  ");
        exit(1);
    }
    const char* tfx ;
    tfx = tf.query();

    //

   
}

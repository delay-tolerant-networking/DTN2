
#include <stdio.h>
#include "debug/Log.h"
#include "bundling/Bundle.h"
//#include "storage/SQLSerialize.h"
#include "storage/PostgresSQLImplementation.h"
#include "storage/MysqlSQLImplementation.h"

#include "storage/SQLStore.h"

//#include "libpq-fe.h"
//#include <libpq++.h>
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
playsql(int i) {

  //  foo o1; o1.id = 771 ; o1.f1 = 123; o1.f2 = 'a'; foo o2;
  
  Bundle o1, o2;  

  o1.source_.set_tuple("bundles://internet/tcp://foo"); o1.bundleid_ = 121; 
  o2.source_.set_tuple("bundles://google/tcp://foo");o2.bundleid_ =  555;

  cout << " read stuff,  \n" << i << endl;
  // cin >> o1.id  >> o1.f1 >> o1.f2 ;
  // cout << "  read end  \n\n" ; 

   const char* table_name = "try";
  
   
 
    SQLImplementation *db ;

    if (i ==1)
   db  =  new PostgresSQLImplementation("cse544");
    else
    db =  new MysqlSQLImplementation("cse544");


  SQLStore *sqlstore = new SQLStore(table_name,&o1,db);
  BundleStore *bstore = new SQLBundleStore(sqlstore);

   int retval2 = bstore->put(&o1,1);
   retval2 = bstore->put(&o2,2);

   cout << " answer is " << retval2 << endl;

   
     Bundle *g1 = bstore->get(1);
      Bundle *g2 = bstore->get(2);
    
     retval2 = bstore->put(g1,3);


   ASSERT (o1.bundleid_ == g1->bundleid_) ; 
     ASSERT (o2.bundleid_ == g2->bundleid_) ; 
   
 
  //db = NULL;
     db->close();
  // PQfinish(pgconn_db);
  exit(0);

}  

int
main(int argc, const char** argv)
{


    Log::init();
    playsql(atoi(argv[1]));
    Bundle b, b2;
    b.bundleid_ = 100;
    ASSERT(!b.source_.valid());
    ASSERT(!b2.source_.valid());
    
    b.source_.set_tuple("bundles://internet/tcp://foo");
    ASSERT(b.source_.valid());
    ASSERT(b.source_.region().compare("internet") == 0);
    ASSERT(b.source_.admin().compare("tcp://foo") == 0);

}

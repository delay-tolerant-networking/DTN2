/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <oasys/debug/Log.h>

#include "bundling/Bundle.h"

#include "storage/PostgresSQLImplementation.h"
#include "storage/MysqlSQLImplementation.h"

#include "storage/SQLBundleStore.h"




class foo : public SerializableObject, public Logger {
public:
  foo() : Logger ("/test/foo") {} ;
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

    log_info("this object is %d char is %d  and id is %d \n",f1,f2,id);


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

    const char* database = "dtn";
    
    //  foo o1; o1.id = 771 ; o1.f1 = 123; o1.f2 = 'a'; foo o2;
    
    SQLImplementation *db ;
    
    if (i ==1) 
       db  =  new PostgresSQLImplementation();
    
    else
	db =  new MysqlSQLImplementation();

    db->connect(database);
	   
    db->exec_query("drop table try;");
    
    BundleStore *bstore = new SQLBundleStore(db, "try");
    BundleStore::init(bstore);
    
    
    Bundle o1, o2;
    int id1 = 121;
    int id2 = 555;
   
    o1.source_.assign("bundles://internet/tcp://foo. Hello I'Am Sushant");
    o1.bundleid_ = id1;
    o1.custreq_ = 1;
    o1.fwd_rcpt_ = 1;
    bzero(o1.test_binary_,20);
    o1.test_binary_[0] = 2;
    o1.test_binary_[1] = 5;
    o1.test_binary_[2] = 38;
    o1.test_binary_[3] = o1.test_binary_[1] +     o1.test_binary_[2] ;
    
    o2.source_.assign("bundles://google/tcp://foo");
    o2.bundleid_ =  id2;
    o2.return_rcpt_ = 1;
    
    int retval2 = bstore->put(&o1,id1);
    retval2 = bstore->put(&o2,id2);
    
    Bundle *g1 = bstore->get(id1);
    Bundle *g2 = bstore->get(id2);
    
    retval2 = bstore->put(g1,id1);
    
    ASSERT(o1.bundleid_ == g1->bundleid_);
    ASSERT(o1.custreq_ == g1->custreq_);
    ASSERT(o1.custody_rcpt_ == g1->custody_rcpt_);
    ASSERT(o1.recv_rcpt_ == g1->recv_rcpt_);
    ASSERT(o1.fwd_rcpt_ == g1->fwd_rcpt_);
    ASSERT(o1.return_rcpt_ == g1->return_rcpt_);
    ASSERT(o1.test_binary_[2] == g1->test_binary_[2]);
   
    
    ASSERT (o2.bundleid_ == g2->bundleid_);
    
    
    
    db->close();
    
    exit(0);
    
}  

int
main(int argc, const char** argv)
{


    Log::init(LOG_DEBUG);
    playsql(atoi(argv[1]));
    Bundle b, b2;
    b.bundleid_ = 100;
    ASSERT(!b.source_.valid());
    ASSERT(!b2.source_.valid());
    
    b.source_.assign("bundles://internet/tcp://foo");
    ASSERT(b.source_.valid());
    ASSERT(b.source_.region().compare("internet") == 0);
    ASSERT(b.source_.admin().compare("tcp://foo") == 0);

}

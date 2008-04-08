#include <dtn-config.h>
#include <oasys/util/UnitTest.h>
#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/StringBuffer.h>

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif
#include "naming/EndpointID.h"
#include "prophet/Dictionary.h"
#include "prophet/HelloTLV.h"
#include "prophet/RIBDTLV.h"
#include "prophet/RIBTLV.h"
#include "prophet/BundleTLVEntryList.h"
#include "prophet/OfferTLV.h"
#include "prophet/ResponseTLV.h"
#include "prophet/ProphetTLV.h"
#include "prophet/BundleCore.h"
#include "prophet/Table.h"

using namespace oasys;
using namespace dtn;

ScratchBuffer<u_char*,1024> buf;
u_char* bp = buf.buf(1024);
const int COUNT = 10;
prophet::HelloTLV* hello;
prophet::RIBDTLV* ribd;
prophet::RIBTLV* rib;
prophet::OfferTLV* btlv;

#define CHECK_TLV(a,b,fn) CHECK_EQUAL((a)->fn(),(b)->fn()) 

DECLARE_TEST(HelloTLV) {

    EndpointID eid("dtn://tlvtest");
    hello = new prophet::HelloTLV(prophet::HelloTLV::SYN,100,eid.c_str());
    CHECK(hello->length() > 0);

    // serialize
    size_t tlvsz = hello->serialize(bp,1024);
    CHECK_GT(tlvsz,prophet::HelloTLV::HelloTLVHeaderSize);
    CHECK_EQUAL(tlvsz,hello->length());
    printf("TLV Typecode: 0x%x\n",hello->typecode());
    printf("TLV Flags: 0x%x\n",hello->flags());
    CHECK_EQUAL(prophet::HelloTLV::SYN,hello->hf());
    CHECK_EQUAL(100,hello->timer());
    CHECK_EQUAL(hello->sender().compare(eid.str()),0);

    //deserialize
    prophet::HelloTLV *ht =
        prophet::TLVFactory<prophet::HelloTLV>::deserialize(bp,1024);
    CHECK(ht != NULL);
    CHECK_TLV(hello,ht,typecode);
    CHECK_TLV(hello,ht,flags);
    CHECK_TLV(hello,ht,length);
    CHECK_TLV(hello,ht,hf);
    CHECK_TLV(hello,ht,timer);
    CHECK_EQUAL(hello->sender().compare(ht->sender()),0);
    
    delete ht;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(RIBDTLV) {

    std::string sender("dtn://local");
    std::string receiver("dtn://remote");
    prophet::Dictionary pd(sender,receiver);
    CHECK(pd.assign(std::string("dtn://test"),2));

    // fill in some RIBD
    for (size_t i=0; i < COUNT-2; i++) {
        oasys::StringBuffer str(128,"dtn://node-");
        str.appendf("%zu/test",i);
        pd.assign(str.c_str(),i+3);
    }
    
    ribd = new prophet::RIBDTLV(pd);
    CHECK( ribd->length() > 0 );
    prophet::Dictionary clone(ribd->ribd(sender,receiver));

    for(prophet::Dictionary::const_iterator i = clone.begin();
        i != clone.end(); i++)
    {
        printf("SID %4u, dest_id %20s\n",(*i).first,(*i).second.c_str());
    }

    // serialize
    size_t tlvsz = ribd->serialize(bp,1024);
    CHECK_GT(tlvsz,prophet::RIBDTLV::RIBDTLVHeaderSize);
    CHECK_EQUAL(tlvsz,ribd->length());
    printf("TLV Typecode: 0x%x\n",ribd->typecode());
    printf("TLV Flags: 0x%x\n",ribd->flags());

    // deserialize
    prophet::RIBDTLV* rd =
        prophet::TLVFactory<prophet::RIBDTLV>::deserialize(bp,1024);
    CHECK(rd != NULL);
    CHECK_TLV(ribd,rd,typecode);
    CHECK_TLV(ribd,rd,flags);
    CHECK_TLV(ribd,rd,length);
    prophet::Dictionary pd2(rd->ribd(sender,receiver));
    printf("pd2 %zu pd %zu\n",pd2.size(),pd.size());
    CHECK(pd2.size() == pd.size());
    for(prophet::Dictionary::const_iterator i = pd2.begin();
        i != pd2.end();
        i++)
    {
        u_int16_t sid = (*i).first;
        std::string dest_id = (*i).second;
        CHECK_EQUAL(sid,pd.find(dest_id));
        CHECK_EQUALSTR(dest_id.c_str(),pd.find(sid).c_str());
    }

    delete rd;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(RIBTLV) {

    prophet::Dictionary pd(std::string("dtn://local"),
                           std::string("dtn://remote"));
    prophet::RIBNodeList nodes;
    // fill in some RIBD
    for (size_t i=0; i < COUNT-2; i++) {
        oasys::StringBuffer str(128,"dtn://node-");
        str.appendf("%zu",i);
        prophet::Node n(str.c_str(),i % 3 == 1,i % 3 == 2, i % 3 == 0);
        u_int16_t sid = pd.insert(str.c_str());
        CHECK(sid != prophet::Dictionary::INVALID_SID);
        prophet::RIBNode* node = new prophet::RIBNode(&n,sid);
        node->update_pvalue();
        node->update_pvalue();
        nodes.push_back(node);
    }
    
    rib = new prophet::RIBTLV(nodes,true,false,false);
    CHECK( rib->length() > 0 );

    // serialize
    size_t tlvsz = rib->serialize(bp,1024);
    CHECK_EQUAL(tlvsz,rib->length());
    prophet::RIBNodeList nl = rib->nodes();
    CHECK_EQUAL(nl.size(),nodes.size());
    for(prophet::RIBTLV::iterator i1 = nl.begin(), i2 = nodes.begin();
        i1 != nl.end() && i2 != nodes.end();
        i1++,i2++)
    {
        prophet::RIBNode *j = *i1;
        prophet::RIBNode *k = *i2;
        char jbuf[10], kbuf[10];
        sprintf(jbuf,"%0.2f",j->p_value());
        sprintf(kbuf,"%0.2f",k->p_value());
        CHECK_EQUALSTR( jbuf, kbuf );
        CHECK_TLV(j,k,relay);
        CHECK_TLV(j,k,custody);
        CHECK_TLV(j,k,internet_gw);
    }

    // deserialize
    prophet::RIBTLV* rt =
        prophet::TLVFactory<prophet::RIBTLV>::deserialize(bp,1024);
    CHECK(rt != NULL);
    CHECK_EQUAL(tlvsz,rt->length());
    nl = rt->nodes();
    CHECK_EQUAL(nl.size(),nodes.size());
    for(prophet::RIBTLV::iterator i1 = nl.begin(), i2 = nodes.begin();
        i1 != nl.end() && i2 != nodes.end();
        i1++,i2++)
    {
        prophet::RIBNode *j = *i1;
        prophet::RIBNode *k = *i2;
        char jbuf[10], kbuf[10];
        sprintf(jbuf,"%0.2f",j->p_value());
        sprintf(kbuf,"%0.2f",k->p_value());
        CHECK_EQUALSTR( jbuf, kbuf );
        CHECK_TLV(j,k,relay);
        CHECK_TLV(j,k,custody);
        CHECK_TLV(j,k,internet_gw);
    }

    // test Table::assign
    prophet::BundleCoreTestImpl core("dtn://local");
    prophet::Table t(&core,"t");
    DO(t.assign(rt->nodes(), pd));
    CHECK( t.size() == rt->nodes().size() );
    CHECK( t.find(pd.find(3)) != NULL );

    delete rt;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(BundleTLV) {
    prophet::BundleOfferList bl;
    for(size_t i = 0; i < (size_t) COUNT; i++) {
        CHECK(bl.add_offer((i<<8)+i, i, i, i%2 == 0, i%2 == 1));
    }

    btlv = new prophet::OfferTLV(bl);
    CHECK( btlv->length() > 0 );

    // serialize
    size_t tlvsz = btlv->serialize(bp,1024);
    CHECK_EQUAL(tlvsz, btlv->length());
    prophet::BundleOfferList b2 = btlv->list();
    CHECK_EQUAL(b2.type(), bl.type());
    CHECK_EQUAL(b2.size(), bl.size());
    {
        for(prophet::BundleOfferList::iterator i = bl.begin(), j = b2.begin();
            i != bl.end() && j != b2.end();
            i++, j++)
        {
            CHECK_TLV( *i, *j, type );
            CHECK_TLV( *i, *j, creation_ts );
            CHECK_TLV( *i, *j, sid );
            CHECK_TLV( *i, *j, custody );
            CHECK_TLV( *i, *j, accept );
            CHECK_TLV( *i, *j, ack );
        }
    }

    //deserialize
    prophet::OfferTLV* bt =
        prophet::TLVFactory<prophet::OfferTLV>::deserialize(bp,1024);
    CHECK(bt != NULL);
    CHECK_EQUAL(tlvsz, bt->length());
    prophet::BundleOfferList b3 = bt->list();
    CHECK_EQUAL(b3.type(), bl.type());
    CHECK_EQUAL(b3.size(), bl.size());
    {
        for(prophet::BundleOfferList::iterator i = bl.begin(), j = b3.begin();
            i != bl.end() && j != b3.end();
            i++, j++)
        {
            CHECK_TLV( *i, *j, type );
            CHECK_TLV( *i, *j, creation_ts );
            CHECK_TLV( *i, *j, sid );
            CHECK_TLV( *i, *j, custody );
            CHECK_TLV( *i, *j, accept );
            CHECK_TLV( *i, *j, ack );
        }
    }
    delete bt;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(ProphetTLV) {

    std::string sender("dtn://somehost");
    std::string receiver("dtn://otherhost");
    prophet::ProphetTLV tlv(sender,receiver,
            prophet::ProphetTLV::UnknownResult,8,9,1023);

    // Add Hello
    tlv.add_tlv(hello);
    CHECK_EQUAL(1,tlv.size());
    size_t buflen = tlv.length();
    CHECK_EQUAL(buflen,prophet::ProphetTLV::ProphetHeaderSize +
                       hello->length());

    // Add RIBD
    tlv.add_tlv(ribd);
    CHECK_EQUAL(2,tlv.size());
    CHECK_EQUAL(buflen,tlv.length() - ribd->length());
    buflen = tlv.length();

    // Add RIB
    tlv.add_tlv(rib);
    CHECK_EQUAL(3,tlv.size());
    CHECK_EQUAL(buflen,tlv.length() - rib->length());
    buflen = tlv.length();

    // Add BundleTLV
    tlv.add_tlv(btlv);
    CHECK_EQUAL(4,tlv.size());
    CHECK_EQUAL(buflen,tlv.length() - btlv->length());
    buflen = tlv.length();

    // write out tlv to buffer
    CHECK(tlv.serialize(bp,1024) == buflen);

    // pretend this is a network transfer, instead of memcpy
    u_char buff[1024];
    memcpy(&buff[0],bp,1024);

    // read back tlv from Bundle payload
    prophet::ProphetTLV* pt = NULL;
    CHECK((pt = prophet::ProphetTLV::deserialize("dtn://somehost",
                    "dtn://otherhost",&buff[0],1024)) != NULL);

    CHECK_EQUAL(tlv.length(),pt->length());
    CHECK_EQUAL(pt->size(),tlv.size());

    size_t size = pt->size();

    // Pull off Hello
    prophet::BaseTLV* b = pt->get_tlv();
    CHECK(b != NULL);
    CHECK_LTU(pt->size(),size);
    CHECK_TLV(b,hello,typecode);
    prophet::HelloTLV* h2 = (prophet::HelloTLV*) b;
    CHECK(h2 != hello); // should not point to same memory
    CHECK_TLV(hello,h2,flags);
    CHECK_TLV(hello,h2,length);
    CHECK_TLV(hello,h2,hf);
    CHECK_TLV(hello,h2,timer);
    CHECK(hello->sender().compare(h2->sender()) == 0);

    //delete hello;  //ProphetTLV destructor takes care of this one
    delete h2;

    // Pull off RIBD
    size = pt->size();
    b = pt->get_tlv();
    CHECK(b != NULL);
    CHECK_LTU(pt->size(),size);
    CHECK_TLV(b,ribd,typecode);
    prophet::RIBDTLV* ribd2 = (prophet::RIBDTLV*) b;
    CHECK(ribd2 != ribd);
    CHECK_TLV(ribd,ribd2,flags);
    CHECK_TLV(ribd,ribd2,length);
    prophet::Dictionary pd = ribd->ribd(sender,receiver);
    prophet::Dictionary pd2 = ribd2->ribd(sender,receiver);
    CHECK(pd2.size() == pd.size());
    for(prophet::Dictionary::const_iterator i = pd2.begin();
        i != pd2.end();
        i++)
    {
        u_int16_t sid = (*i).first;
        std::string eid = (*i).second;
        CHECK_EQUAL(sid,pd.find(eid));
        CHECK_EQUALSTR(eid.c_str(),pd.find(sid).c_str());
    }
    //delete ribd; //ProphetTLV destructor takes care of this one
    delete ribd2;

    // Pull off RIB
    size = pt->size();
    b = pt->get_tlv();
    CHECK(b != NULL);
    CHECK_LTU(pt->size(),size);
    CHECK_TLV(b,rib,typecode);
    prophet::RIBTLV* rib2 = (prophet::RIBTLV*) b;
    prophet::RIBNodeList n1 = rib->nodes();
    prophet::RIBNodeList n2 = rib2->nodes();
    for(prophet::RIBTLV::iterator i1 = n1.begin(), i2 = n2.begin();
        i1 != n1.end() && i2 != n2.end();
        i1++,i2++)
    {
        prophet::RIBNode *j = *i1;
        prophet::RIBNode *k = *i2;
        char jbuf[10], kbuf[10];
        sprintf(jbuf,"%0.2f",j->p_value());
        sprintf(kbuf,"%0.2f",k->p_value());
        CHECK_EQUALSTR( jbuf, kbuf );
        CHECK_TLV(j,k,relay);
        CHECK_TLV(j,k,custody);
        CHECK_TLV(j,k,internet_gw);
    }

    //delete rib; //ProphetTLV destructor takes care of this one
    delete rib2;

    // Pull off BundleTLV
    size = pt->size();
    b = pt->get_tlv();
    CHECK(b != NULL);
    CHECK_LTU(pt->size(),size);
    CHECK_TLV(b,btlv,typecode);
    prophet::OfferTLV* btlv2 = (prophet::OfferTLV*) b;
    prophet::BundleOfferList b1 = btlv->list();
    prophet::BundleOfferList b2 = btlv2->list();
    {
        for(prophet::BundleOfferList::iterator i = b1.begin(), j = b2.begin();
            i != b1.end() && j != b2.end();
            i++, j++)
        {
            CHECK_TLV( *i, *j, creation_ts );
            CHECK_TLV( *i, *j, sid );
            CHECK_TLV( *i, *j, custody );
            CHECK_TLV( *i, *j, accept );
            CHECK_TLV( *i, *j, ack );
        }
    }

    //delete btlv; //ProphetTLV destructor takes care of this one
    delete btlv2;

    delete pt;

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(ProphetTLVTest) {
    ADD_TEST(HelloTLV);
    ADD_TEST(RIBDTLV);
    ADD_TEST(RIBTLV);
    ADD_TEST(BundleTLV);
    ADD_TEST(ProphetTLV);
}

DECLARE_TEST_FILE(ProphetTLVTest, "prophet tlv parser test");

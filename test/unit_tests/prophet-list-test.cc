#include <dtn-config.h>
#include <oasys/util/UnitTest.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <oasys/debug/Log.h>
#include <oasys/util/StringBuffer.h>

#include "prophet/Ack.h"
#include "prophet/Node.h"
#include "prophet/Table.h"
#include "prophet/AckList.h"
#include "prophet/Dictionary.h"
#include "prophet/BundleCore.h"
#include "prophet/BundleTLVEntry.h"
#include "prophet/BundleTLVEntryList.h"

#include "prophet/PointerList.h"

using namespace oasys;

size_t test_iterations = 10;
size_t table_iterations = 100;

DECLARE_TEST(NodeList) {
    prophet::NodeList list;
    prophet::Node* node;

    for (size_t i=0; i < test_iterations; i++) {
        oasys::StringBuffer str(128,"dtn://node-");
        str.appendf("%zu",i);
        node = new prophet::Node(str.c_str());
        CHECK_EQUAL(i,list.size());
        list.push_back(node);
        CHECK(i != list.size());
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Dictionary) {
    prophet::Dictionary pd("dtn://sender","dtn://receiver");
    size_t testsize = test_iterations;
    u_int16_t sid = 0xffff;

    // By definition, sender is string_id 0
    DO(sid = pd.find("dtn://sender"));
    CHECK_EQUAL(sid,0);
    CHECK_EQUALSTR("dtn://sender",pd.find(0).c_str());

    // By definition, receiver is string_id 1
    DO(sid = pd.find("dtn://receiver"));
    CHECK_EQUAL(sid,1);
    CHECK_EQUALSTR("dtn://receiver",pd.find(1).c_str());

    std::string tester;
    for (size_t i=0; i<testsize; i++) {
        oasys::StringBuffer str(128,"dtn://node-");
        str.appendf("%zu/test",i);
        sid = pd.insert(str.c_str());
        CHECK(sid != 0);
        tester.assign(pd.find(sid));
        CHECK_EQUALSTR(tester.c_str(),str.c_str());
    }

    CHECK(pd.size() == testsize);

    prophet::Dictionary pd2(pd);

    CHECK_EQUAL(pd.size(),pd2.size());

    std::string iter;
    for(size_t i = 0; i<pd2.size(); i++) {
        tester.assign(pd.find(i));
        iter.assign(pd2.find(i));
        CHECK_EQUALSTR(tester.c_str(),iter.c_str());
    }

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Table) {
    prophet::BundleCoreTestImpl core;
    prophet::Table pt(&core,"pt");

    std::string a("dtn://test-a"), b("dtn://test-b");

    CHECK(pt.find(a) == NULL);
    CHECK(pt.p_value(a) == 0.0);
    DO(pt.update_route(a,true,true,false));
    CHECK(pt.p_value(a) == 0.75);

    CHECK(pt.find(b) == NULL);
    CHECK(pt.p_value(b) == 0.0);
    DO(pt.update_transitive(b,a,0.75,true,true,false));
    CHECK(pt.p_value(b) > 0.0);
    printf("pt.p_value(b) == %.02f\n",pt.p_value(b));

    prophet::Table *p2 = NULL;
    {
        prophet::Table p3(&core,"p2");
        p3.update_route(a);
        p3.update_route(b);
        p3.update_route(b);
        p2 = new prophet::Table(p3);
    }
    CHECK(p2->p_value(a) == 0.75);
    CHECK(p2->p_value(b) >= 0.75);
    delete p2;

    // default is 0, quota disabled
    prophet::Table p3(&core,"p3");

    for (size_t i=0; i<table_iterations; i++) {
        oasys::StringBuffer str(128,"dtn://node-");
        str.appendf("%zu",i);
        p3.update_route(str.c_str());
        int j = i % 10;
        while (j-- > 0) {
            p3.update_route(str.c_str());
        }
    }

    p3.set_max_route(table_iterations/2);
    CHECK_EQUAL(p3.size(), table_iterations/2);

    prophet::Table::heap_iterator hi = p3.heap_begin();
    std::vector<prophet::Node*> list(p3.heap_begin(),p3.heap_end());
    struct prophet::heap_compare c;
    CHECK( (prophet::Heap<prophet::Node*,
                          std::vector<prophet::Node*>,
                          struct prophet::heap_compare>::is_heap(list,c)) ); 
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(BundleTLVList) {
    prophet::BundleOfferList bol;
    for(size_t i = test_iterations; i>0; --i) {
        CHECK(bol.add_offer(0xffff+i,i,i,false,false));
        CHECK(!bol.add_offer(0xffff+i,i,i,true,true));
        CHECK_LTU(bol.size(),test_iterations - i + 1);
    }

    prophet::BundleResponseList brl;
    for(size_t i = test_iterations; i>0; --i) {
        CHECK(brl.add_response(0xffff+i,i,i,false,true));
        CHECK(!brl.add_response(0xffff+i,i,i,true,false));
        CHECK_LTU(brl.size(),test_iterations - i + 1);
    }

    CHECK(bol.size() == test_iterations);
    for(prophet::BundleOfferList::iterator i = bol.begin();
        i != bol.end();
        i++)
    {
        prophet::BundleOfferEntry* bo = *i;
        CHECK(bol.find(bo->creation_ts(),bo->seqno(),bo->sid()) != NULL);
    }
    bol.clear();
    CHECK(bol.size() == 0);
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(AckList) {
    prophet::PointerList<prophet::Ack> clone;
    prophet::AckList pl;
    for(size_t i = 0; i<test_iterations; i++) {
        oasys::StringBuffer str(128,"dtn://node-");
        str.appendf("%zu",i);
        CHECK(pl.insert(str.c_str(),0xff00+i,i,test_iterations-i));
        prophet::Ack* pa = new prophet::Ack(str.c_str(),0xff00+i,i,test_iterations-i);
        CHECK(!pl.insert(pa));
        delete pa;

        CHECK(pl.size() == i+1);

        CHECK(pl.is_ackd(str.c_str(),0xff00+i,i));

        CHECK(pl.fetch(str.c_str(),&clone) == 1);
        pa = clone.front();
        CHECK_EQUALSTR(str.c_str(), pa->dest_id().c_str());
        clone.clear();
    }
    CHECK_EQUAL(pl.size(),test_iterations);
    CHECK_EQUAL(pl.expire(),test_iterations);
    CHECK_EQUAL(0,pl.size());
    CHECK(pl.empty());
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(ProphetListTest) {
    ADD_TEST(NodeList);
    ADD_TEST(Dictionary);
    ADD_TEST(Table);
    ADD_TEST(BundleTLVList);
    ADD_TEST(AckList);
}

DECLARE_TEST_FILE(ProphetListTest, "prophet list test");

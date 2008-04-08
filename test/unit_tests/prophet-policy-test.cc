#include <dtn-config.h>
#include <oasys/util/UnitTest.h>
#include <sys/types.h>
#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/debug/Log.h>

#include "prophet/Node.h"
#include "prophet/Table.h"
#include "prophet/Bundle.h"
#include "prophet/Stats.h"
#include "prophet/QueuePolicy.h"

using namespace oasys;

/*
   All the Queue Policy comparators are tuned for use in priority_queue,
   to effect eviction in the described order (see the Prophet I-D for
   details).
 */

DECLARE_TEST(FIFO) {
    //                       dest_id, cts, seq, ets, size, num_fwd
    prophet::BundleImpl m("dtn://test-a",0xfe,   1,  60, 1024,      0);
    prophet::BundleImpl n("dtn://test-b",0xff,   2,  60,  512,      1);

    // create FIFO comparator
    prophet::QueueComp* c =
        prophet::QueuePolicy::policy(prophet::QueuePolicy::FIFO);

    CHECK( c != NULL );
    CHECK_EQUAL( c->qp(), prophet::QueuePolicy::FIFO );

    // should test out n < m, since m is newer than n
    CHECK( c->operator()(&n,&m) );

    delete c;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MOFO) {
    //                       dest_id, cts, seq, ets, size, num_fwd
    prophet::BundleImpl m("dtn://test-a",0xfe,   1,  60, 1024,      0);
    prophet::BundleImpl n("dtn://test-b",0xff,   2,  60,  512,      1);

    // create MOFO comparator
    prophet::QueueComp* c = 
        prophet::QueuePolicy::policy(prophet::QueuePolicy::MOFO);

    CHECK( c != NULL );
    CHECK_EQUAL( c->qp(), prophet::QueuePolicy::MOFO );

    CHECK( c->operator()(&m,&n) );

    // change num forward on m
    DO( m.set_num_forward(10) );

    CHECK( c->operator()(&n,&m) );
    delete c;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MOPR) {
    //                       dest_id, cts, seq, ets, size, num_fwd
    prophet::BundleImpl m("dtn://test-a",0xfe,   1,  60, 1024,      0);
    prophet::BundleImpl n("dtn://test-b",0xff,   2,  60,  512,      1);

    prophet::Stats s;

    // create MOPR comparator
    prophet::QueueComp* c = 
        prophet::QueuePolicy::policy(prophet::QueuePolicy::MOPR,&s);

    CHECK( c != NULL );
    CHECK_EQUAL( c->qp(), prophet::QueuePolicy::MOPR );

    // should test out equal
    CHECK( !(c->operator()(&m,&n)) && !(c->operator()(&n,&m)) );

    DO( s.update_stats(&m,0.75) );

    CHECK( s.get_mopr(&n) < s.get_mopr(&m) );
    CHECK( c->operator()(&n,&m) );
    delete c;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(LMOPR) {
    //                       dest_id, cts, seq, ets, size, num_fwd
    prophet::BundleImpl m("dtn://test-a",0xfe,   1,  60, 1024,      0);
    prophet::BundleImpl n("dtn://test-b",0xff,   2,  60,  512,      1);

    prophet::Stats s;

    // create LMOPR comparator
    prophet::QueueComp* c = 
        prophet::QueuePolicy::policy(prophet::QueuePolicy::LINEAR_MOPR,&s);

    CHECK( c != NULL );
    CHECK_EQUAL( c->qp(), prophet::QueuePolicy::LINEAR_MOPR );

    // should test out equal
    CHECK( !(c->operator()(&m,&n)) && !(c->operator()(&n,&m)) );

    DO( s.update_stats(&m,0.75) );

    CHECK( s.get_lmopr(&n) < s.get_lmopr(&m) );
    CHECK( c->operator()(&n,&m) );
    delete c;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(SHLI) {
    //                       dest_id, cts, seq, ets, size, num_fwd
    prophet::BundleImpl m("dtn://test-a",0xff,   1,  60, 1024,      0);
    prophet::BundleImpl n("dtn://test-b",0xff,   2,  60,  512,      1);

    // create SHLI comparator
    prophet::QueueComp* c = 
        prophet::QueuePolicy::policy(prophet::QueuePolicy::SHLI);

    CHECK( c != NULL );
    CHECK_EQUAL( c->qp(), prophet::QueuePolicy::SHLI );

    // should test out equal
    CHECK( !(c->operator()(&m,&n)) && !(c->operator()(&n,&m)) );

    // m's expiration now occurs after n's,
    // so n should come before m in eviction order
    DO( m.set_expiration_ts(120) );

    c->verbose_ = true;
    CHECK( c->operator()(&m,&n) );
    delete c;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(LEPR) {
    //                       dest_id, cts, seq, ets, size, num_fwd
    prophet::BundleImpl m("dtn://test-a",0xff,   1,  60, 1024,      0);
    prophet::BundleImpl n("dtn://test-b",0xff,   2,  60,  512,      1);

    const prophet::Node* a;
    prophet::BundleCoreTestImpl core;
    prophet::Table t(&core,"t");
    DO( t.update(new prophet::Node("dtn://test-a", true, true, false)) );
    DO( t.update(new prophet::Node("dtn://test-b", true, true, false)) );

    // create LEPR comparator
    u_int minfwd = 3;
    prophet::QueueComp* c = 
        prophet::QueuePolicy::policy(prophet::QueuePolicy::LEPR,
                                     NULL,&t,minfwd);

    CHECK( c != NULL );
    CHECK_EQUAL( c->qp(), prophet::QueuePolicy::LEPR );

    // should test out equal
    CHECK( !(c->operator()(&m,&n)) && !(c->operator()(&n,&m)) );

    DO( a = t.find("dtn://test-a") );
    CHECK( a != NULL );
    prophet::Node* b;
    DO( b = new prophet::Node(*a) );
    DO( b->update_pvalue() ); // route to test-a has p_value == 0.75
    DO( t.update(b) );

    CHECK( c->operator()(&m,&n) );
    delete c;

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(ProphetPolicyTest) {
    ADD_TEST(FIFO);
    ADD_TEST(MOFO);
    ADD_TEST(MOPR);
    ADD_TEST(LMOPR);
    ADD_TEST(SHLI);
    ADD_TEST(LEPR);
}

DECLARE_TEST_FILE(ProphetPolicyTest, "prophet queue policy comparator test");

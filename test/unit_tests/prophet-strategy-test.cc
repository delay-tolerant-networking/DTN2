#include <dtn-config.h>
#include <oasys/util/UnitTest.h>
#include <oasys/debug/Log.h>

#include "prophet/Node.h"
#include "prophet/Table.h"
#include "prophet/Bundle.h"
#include "prophet/Stats.h"
#include "prophet/FwdStrategy.h"

using namespace oasys;

//                       dest_id, cts, seq, ets, size, num_fwd
prophet::BundleImpl m("dtn://test-a",0xfe,   1,  60, 1024,      0);
prophet::BundleImpl n("dtn://test-b",0xff,   2,  60,  512,      1);

prophet::BundleCoreTestImpl core;

DECLARE_TEST(GRTR) {
    prophet::FwdStrategyComp* c =
        prophet::FwdStrategy::strategy(prophet::FwdStrategy::GRTR);

    CHECK( c != NULL );
    CHECK_EQUAL(c->fwd_strategy(), prophet::FwdStrategy::GRTR);
    CHECK( !c->operator()(&m,&n) );
    CHECK( c->operator()(&n,&m) );

    delete c;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GTMX) {
    prophet::FwdStrategyComp* c =
        prophet::FwdStrategy::strategy(prophet::FwdStrategy::GTMX);

    CHECK( c != NULL );
    CHECK_EQUAL(c->fwd_strategy(), prophet::FwdStrategy::GTMX);
    CHECK( !c->operator()(&m,&n) );
    CHECK( c->operator()(&n,&m) );

    delete c;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GRTR_PLUS) {
    prophet::FwdStrategyComp* c =
        prophet::FwdStrategy::strategy(prophet::FwdStrategy::GRTR_PLUS);

    CHECK( c != NULL );
    CHECK_EQUAL(c->fwd_strategy(), prophet::FwdStrategy::GRTR_PLUS);
    CHECK( !c->operator()(&m,&n) );
    CHECK( c->operator()(&n,&m) );

    delete c;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GTMX_PLUS) {
    prophet::FwdStrategyComp* c =
        prophet::FwdStrategy::strategy(prophet::FwdStrategy::GTMX_PLUS);

    CHECK( c != NULL );
    CHECK_EQUAL(c->fwd_strategy(), prophet::FwdStrategy::GTMX_PLUS);
    CHECK( !c->operator()(&m,&n) );
    CHECK( c->operator()(&n,&m) );

    delete c;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GRTR_SORT) {
    prophet::Table t1(&core,"t1"), t2(&core,"t2");

    prophet::FwdStrategyComp* c =
        prophet::FwdStrategy::strategy(prophet::FwdStrategy::GRTR_SORT,&t1,&t2);

    CHECK( c != NULL );
    CHECK_EQUAL(c->fwd_strategy(), prophet::FwdStrategy::GRTR_SORT);
    CHECK( !c->operator()(&m,&n) );
    CHECK( !c->operator()(&n,&m) );

    DO (t2.update_route(m.destination_id()));
    CHECK( !c->operator()(&m,&n) );
    CHECK( c->operator()(&n,&m) );

    delete c;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GRTR_MAX) {
    prophet::Table t1(&core,"t1");

    prophet::FwdStrategyComp* c =
        prophet::FwdStrategy::strategy(prophet::FwdStrategy::GRTR_MAX,NULL,&t1);

    CHECK( c != NULL );
    CHECK_EQUAL(c->fwd_strategy(), prophet::FwdStrategy::GRTR_MAX);
    CHECK( !c->operator()(&m,&n) );
    CHECK( !c->operator()(&n,&m) );

    DO (t1.update_route(m.destination_id()));
    CHECK( !c->operator()(&m,&n) );
    CHECK( c->operator()(&n,&m) );

    delete c;

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(ProphetStrategyTest) {
    ADD_TEST(GRTR);
    ADD_TEST(GTMX);
    ADD_TEST(GRTR_PLUS);
    ADD_TEST(GTMX_PLUS);
    ADD_TEST(GRTR_SORT);
    ADD_TEST(GRTR_MAX);
}

DECLARE_TEST_FILE(ProphetStrategyTest, "prophet fwd strategy test");

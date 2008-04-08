#include <dtn-config.h>
#include <oasys/util/UnitTest.h>

#include "prophet/Node.h"
#include "prophet/Link.h"
#include "prophet/Table.h"
#include "prophet/Bundle.h"
#include "prophet/Stats.h"
#include "prophet/Decider.h"

using namespace oasys;

//                       dest_id, cts, seq, ets, size, num_fwd
prophet::BundleImpl m("dtn://test-a",0xfe,   1,  60, 1024,      1);
prophet::BundleImpl n("dtn://test-b",0xff,   2,  60,  512,      1);
prophet::BundleCoreTestImpl core;
prophet::LinkImpl nexthop("dtn://nexthop");

DECLARE_TEST(GRTR) {
    prophet::Table local(&core,"local"), remote(&core,"remote");
    prophet::Stats stats;
    prophet::Decider* d =
        prophet::Decider::decider(prophet::FwdStrategy::GRTR,
                                  &nexthop, &core, &local, &remote,
                                  &stats, 0, true);

    CHECK( d != NULL );
    CHECK_EQUAL(d->fwd_strategy(), prophet::FwdStrategy::GRTR);
    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    // install a route to m's destination in nexthop's table
    DO( remote.update_route("dtn://test-a") );

    CHECK(   d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    delete d;

    // try again with new decider, not relay
    d = prophet::Decider::decider(prophet::FwdStrategy::GRTR,
                                  &nexthop, &core, &local, &remote,
                                  &stats, 0, false);

    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    delete d;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GTMX) {
    prophet::Table local(&core,"local"), remote(&core,"remote");
    prophet::Stats stats;
    prophet::Decider* d =
        prophet::Decider::decider(prophet::FwdStrategy::GTMX,
                                  &nexthop, &core, &local, &remote,
                                  &stats, 1, true);

    CHECK( d != NULL );
    CHECK_EQUAL(d->fwd_strategy(), prophet::FwdStrategy::GTMX);
    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    // install a route to m's destination in nexthop's table
    DO( remote.update_route("dtn://test-a") );

    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    // try again with MAX_FWD > 1
    delete d;

    d = prophet::Decider::decider(prophet::FwdStrategy::GTMX,
                                  &nexthop, &core, &local, &remote,
                                  &stats, 3, true);

    CHECK(   d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    delete d;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GRTR_PLUS) {
    prophet::Table local(&core,"local"), remote(&core,"remote");
    prophet::Stats stats;
    prophet::Decider* d =
        prophet::Decider::decider(prophet::FwdStrategy::GRTR_PLUS,
                                  &nexthop, &core, &local, &remote,
                                  &stats, 0, true);

    CHECK( d != NULL );
    CHECK_EQUAL(d->fwd_strategy(), prophet::FwdStrategy::GRTR_PLUS);
    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    // install a route to m's destination in nexthop's table
    DO( remote.update_route("dtn://test-a") );

    CHECK(   d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    // try again with new stats
    DO( stats.update_stats(&m,0.9) );

    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    delete d;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GTMX_PLUS) {
    prophet::Table local(&core,"local"), remote(&core,"remote");
    prophet::Stats stats;
    prophet::Decider* d =
        prophet::Decider::decider(prophet::FwdStrategy::GTMX_PLUS,
                                  &nexthop, &core, &local, &remote,
                                  &stats, 1, true);

    CHECK( d != NULL );
    CHECK_EQUAL(d->fwd_strategy(), prophet::FwdStrategy::GTMX_PLUS);
    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    // install a route to m's destination in nexthop's table
    DO( remote.update_route("dtn://test-a") );

    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    // try again with MAX_FWD > 1
    delete d;
    DO( d = prophet::Decider::decider(prophet::FwdStrategy::GTMX_PLUS,
                                  &nexthop, &core, &local, &remote,
                                  &stats, 3, true) );

    CHECK(   d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    // try again with new stats
    DO( stats.update_stats(&m,0.9) );

    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    delete d;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GRTR_SORT) {
    prophet::Table local(&core,"local"), remote(&core,"remote");
    prophet::Stats stats;
    prophet::Decider* d =
        prophet::Decider::decider(prophet::FwdStrategy::GRTR_SORT,
                                  &nexthop, &core, &local, &remote,
                                  &stats, 0, true);

    CHECK( d != NULL );
    CHECK_EQUAL(d->fwd_strategy(), prophet::FwdStrategy::GRTR_SORT);
    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    // install a route to m's destination in nexthop's table
    DO( remote.update_route("dtn://test-a") );

    CHECK(   d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    delete d;

    // try again with new decider, not relay
    d = prophet::Decider::decider(prophet::FwdStrategy::GRTR,
                                  &nexthop, &core, &local, &remote,
                                  &stats, 0, false);

    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    delete d;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(GRTR_MAX) {
    prophet::Table local(&core,"local"), remote(&core,"remote");
    prophet::Stats stats;
    prophet::Decider* d =
        prophet::Decider::decider(prophet::FwdStrategy::GRTR_MAX,
                                  &nexthop, &core, &local, &remote,
                                  &stats, 0, true);

    CHECK( d != NULL );
    CHECK_EQUAL(d->fwd_strategy(), prophet::FwdStrategy::GRTR_MAX);
    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    // install a route to m's destination in nexthop's table
    DO( remote.update_route("dtn://test-a") );

    CHECK(   d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    delete d;

    // try again with new decider, not relay
    d = prophet::Decider::decider(prophet::FwdStrategy::GRTR,
                                  &nexthop, &core, &local, &remote,
                                  &stats, 0, false);

    CHECK( ! d->operator()(&m) );
    CHECK( ! d->operator()(&n) );

    delete d;

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

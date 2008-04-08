#include <dtn-config.h>
#include <oasys/util/UnitTest.h>
#include <sys/types.h>
#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/debug/Log.h>

#include "prophet/BundleImpl.h"
#include "prophet/Stats.h"

using namespace oasys;


DECLARE_TEST(Stats) {
    //                     dest_id,      cts, seq, ets, size, num_fwd
    prophet::BundleImpl m("dtn://test-a",0xfe,  1,  60, 1024,      0);
    prophet::BundleImpl n("dtn://test-b",0xff,  2,  60,  512,      1);
    prophet::Stats s;

    CHECK(s.get_p_max(&n) == s.get_p_max(&m));
    CHECK(s.get_mopr(&n) == s.get_mopr(&n));
    CHECK(s.get_lmopr(&m) == s.get_lmopr(&n));
    s.update_stats(&m,0.75);
    CHECK(s.get_p_max(&m) > s.get_p_max(&n));
    CHECK(s.get_mopr(&m) > s.get_mopr(&n));
    CHECK(s.get_lmopr(&m) > s.get_lmopr(&n));
    s.drop_bundle(&m);
    CHECK_EQUAL(s.dropped(), 1);
    CHECK(s.get_p_max(&n) == s.get_p_max(&m));
    CHECK(s.get_mopr(&n) == s.get_mopr(&n));
    CHECK(s.get_lmopr(&m) == s.get_lmopr(&n));

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(ProphetStatsTest) {
    ADD_TEST(Stats);
}

DECLARE_TEST_FILE(ProphetStatsTest, "prophet stats test");

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif
#include <oasys/util/UnitTest.h>
#include <netinet/in.h>
#include "prophet/Node.h"
#include "prophet/BundleTLVEntry.h"
#include "prophet/Ack.h"
#include <sys/types.h>
#include <oasys/util/ScratchBuffer.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/Time.h>
#include <oasys/debug/Log.h>

using namespace oasys;

size_t test_iterations = 10;

DECLARE_TEST(NodeTest) {
    prophet::Node a("dtn://test",true,false,false);

    const char* str = a.dest_id();
    CHECK_EQUALSTR(str,"dtn://test");

    a.update_pvalue();
    CHECK(prophet::NodeParams::DEFAULT_P_ENCOUNTER == a.p_value());

    CHECK(a.relay());
    CHECK(!a.custody());
    CHECK(!a.internet_gw());

    oasys::Time time;
    time.get_time();
    CHECK( a.age() <= time.sec_ );

    prophet::Node b;
    struct prophet::NodeParams c(*(b.params()));
    CHECK(prophet::NodeParams::DEFAULT_P_ENCOUNTER == c.encounter_);
    CHECK(prophet::NodeParams::DEFAULT_BETA        == c.beta_);
    CHECK(prophet::NodeParams::DEFAULT_GAMMA       == c.gamma_);
    CHECK(prophet::NodeParams::DEFAULT_KAPPA       == c.kappa_);

    double prev = b.p_value();
    b.update_pvalue();
    CHECK(prev < b.p_value());
    prev = b.p_value();
    b.update_age();
    CHECK(prev <= b.p_value());

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(BundleTLVEntryTest) {
    prophet::BundleTLVEntry* a =
        prophet::BundleTLVEntry::create_entry(0xffffffff,0,0xffff,
                                              false,false,false);
    CHECK(a != NULL);
    prophet::BundleTLVEntry* b =
        prophet::BundleTLVEntry::create_entry(0,0,0,
                                              false,false,false);
    CHECK(b != NULL);
    CHECK( (*b < *a) && ! (*a < *b) );
    const char *atype = prophet::BundleOfferEntry::type_to_str(a->type());
    CHECK_EQUALSTR("OFFER",atype);

    prophet::BundleTLVEntry c(*b);
    CHECK( ! (c < *b) && ! (*b < c) );

    prophet::BundleTLVEntry* d =
        prophet::BundleTLVEntry::create_entry(0xff,0,0xf,false,true,false);

    CHECK_EQUALSTR("RESPONSE",
                   prophet::BundleOfferEntry::type_to_str(d->type()));
    CHECK( ! d->custody());
    CHECK( d->accept());
    CHECK( ! d->ack());

    delete a;
    delete b;
    delete d;
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(AckTest) {
    prophet::Ack a("dtn://dtn-1");
    prophet::Ack b("dtn://dtn-1");
    CHECK( ! (a < b) && ! (b < a) );
    a.set_cts(0xffff);
    CHECK( (a < b) || (b < a) );
    a.set_cts(0);
    CHECK( ! (a < b) && ! (b < a) );
    a.set_seq(0xff);
    CHECK( (a < b) || (b < a) );
    a.set_seq(0);
    CHECK( ! (a < b) && ! (b < a) );
    a.set_dest_id("dtn://dtn-2");
    CHECK( (a < b) || (b < a) );
    prophet::Ack c(a);
    CHECK( ! (a < c) && ! (c < a) );
    c.set_cts(0xffff);
    CHECK( (a < c) || (c < a) );
    a = c;
    CHECK( ! (a < c) && ! (c < a) );
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(ProphetNodeTest) {
    ADD_TEST(NodeTest);
    ADD_TEST(BundleTLVEntryTest);
    ADD_TEST(AckTest);
}

DECLARE_TEST_FILE(ProphetNodeTest, "prophet node test");

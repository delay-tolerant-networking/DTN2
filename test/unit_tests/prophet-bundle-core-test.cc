#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif
#include <oasys/util/UnitTest.h>
#include <oasys/thread/SpinLock.h>
#include "conv_layers/NullConvergenceLayer.h"
#include "contacts/Link.h"
#include "routing/ProphetBundle.h"
#include "routing/ProphetBundleList.h"
#include "routing/ProphetBundleCore.h"
#include "bundling/Bundle.h"
#include "bundling/BundleActions.h"

using namespace oasys;

dtn::Bundle *b;
dtn::BundleRef br("temp test ref");
dtn::LinkRef l;
dtn::NullConvergenceLayer *cl = NULL;
dtn::BundleActions actions;
oasys::SpinLock lock;
dtn::ProphetBundleCore core(oasys::Builder::builder());

DECLARE_TEST(ProphetBundleCore)
{
    prophet::LinkImpl link("dtn://remotehost");
    dtn::ProphetBundleCore core("dtn://localhost",&actions,&lock);
    CHECK( core.is_route("dtn://localhost/prophet","dtn://localhost"));
    CHECK(!core.is_route("dtn://remotehost","dtn://localhost"));
    CHECK_EQUALSTR(core.prophet_id().c_str(), "dtn://localhost/prophet");
    CHECK_EQUALSTR(core.prophet_id(&link).c_str(), "dtn://remotehost/prophet");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(ProphetBundleWrapper)
{
    b = new dtn::Bundle(oasys::Builder::builder());
    b->test_set_bundleid(0);
    b->mutable_payload()->init(0, dtn::BundlePayload::NODATA);
    b->add_ref("test");
    b->mutable_dest()->assign("dtn://dtn.howdy.dtn");
    b->mutable_source()->assign("dtn://dtn.mysrc.dtn");
    b->set_creation_ts(dtn::BundleTimestamp(98765, 4321));
    b->set_expiration(123);
    b->set_custody_requested(true);

    br = b;
    CHECK(br.object() != NULL);
    dtn::ProphetBundle p(br);
    CHECK_EQUALSTR(b->dest().c_str(), p.destination_id().c_str());
    CHECK_EQUALSTR(b->source().c_str(), p.source_id().c_str());
    CHECK_EQUAL(b->creation_ts().seconds_, p.creation_ts());
    CHECK_EQUAL(b->creation_ts().seqno_, p.sequence_num());
    CHECK_EQUAL(b->payload().length(), p.size());
    CHECK_EQUAL(p.num_forward(), 0);
    CHECK(p.custody_requested());

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(ProphetBundleList)
{
    dtn::ProphetBundleList bl(&core);
    CHECK(br.object() != NULL);
    bl.add(br);
    const prophet::Bundle* res = bl.find("dtn://dtn.howdy.dtn",
                                         98765, 4321);
    CHECK( res != NULL );

    dtn::BundleRef bres("test finder temp");
    bres = bl.find_ref(res);
    CHECK( bres.object() != NULL );

    bl.clear();
    CHECK(bl.find("dtn://dtn.howdy.dtn",98765,4321) == NULL);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(ProphetLinkWrapper)
{
    cl = new dtn::NullConvergenceLayer();;
    l = dtn::Link::create_link("l", dtn::Link::OPPORTUNISTIC, cl,
                               "ever-so-where-i-go", 0, NULL);
    l->set_remote_eid(std::string("dtn://whodat.over.der.dtn"));
    dtn::ProphetLink pl(l);
    CHECK_EQUALSTR(l->nexthop(),pl.nexthop());
    CHECK_EQUALSTR(l->remote_eid().c_str(),pl.remote_eid());

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(ProphetLinkList)
{
    dtn::ProphetLinkList pll;
    pll.add(l);
    CHECK( pll.find("dtn://whodat.over.der.dtn") != NULL );
    dtn::LinkRef tmp("ProphetLinkList");
    tmp = pll.find_ref("dtn://whodat.over.der.dtn");
    CHECK( tmp.object() != NULL );
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(CleanErUpper)
{
    br = NULL;
    cl->delete_link(l);
    l = NULL;
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(ProphetBundleCoreTester)
{
    ADD_TEST(ProphetBundleCore);
    ADD_TEST(ProphetBundleWrapper);
    ADD_TEST(ProphetBundleList);
    ADD_TEST(ProphetLinkWrapper);
    ADD_TEST(ProphetLinkList);
    ADD_TEST(CleanErUpper);
}

DECLARE_TEST_FILE(ProphetBundleCoreTester, "prophet bundle core test");

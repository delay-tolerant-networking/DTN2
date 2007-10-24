#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif
#include <oasys/util/UnitTest.h>
#include <oasys/thread/SpinLock.h>
#include "routing/ProphetBundleCore.h"
#include "bundling/BundleActions.h"

using namespace oasys;

DECLARE_TEST(ProphetBundleCore)
{
    dtn::BundleActions actions;
    oasys::SpinLock* lock = new oasys::SpinLock();
    dtn::ProphetBundleCore core("dtn://localhost",&actions,lock);
    prophet::LinkImpl link("dtn://remotehost");
    CHECK( core.is_route("dtn://localhost/prophet","dtn://localhost"));
    CHECK(!core.is_route("dtn://remotehost","dtn://localhost"));
    CHECK_EQUALSTR(core.prophet_id().c_str(), "dtn://localhost/prophet");
    CHECK_EQUALSTR(core.prophet_id(&link).c_str(), "dtn://remotehost/prophet");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(ProphetBundleCore)
{
    ADD_TEST(ProphetBundleCore);
}

DECLARE_TEST_FILE(ProphetBundleCore, "prophet bundle core test");

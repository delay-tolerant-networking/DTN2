
#include "TclRegistration.h"
#include "RegistrationTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleList.h"

TclRegistration::TclRegistration(u_int32_t regid,
                                 const BundleTuplePattern& endpoint)
    : Registration(regid, endpoint, Registration::ABORT)
{
    logpathf("/registration/logging/%d", regid);
    set_active(true);

    if (! RegistrationTable::instance()->add(this)) {
        log_err("unexpected error adding registration to table");
    }

    log_info("new logging registration on endpoint %s", endpoint.c_str());
}

void
TclRegistration::run()
{
    Bundle* b;
    while (1) {
        b = bundle_list_->pop_blocking();

        
        
        BundleForwarder::post(
            new BundleTransmittedEvent(b, this, b->payload_.length(), true));
        
        b->del_ref();
    }
}

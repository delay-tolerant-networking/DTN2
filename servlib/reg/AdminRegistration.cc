
#include "AdminRegistration.h"
#include "RegistrationTable.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleProtocol.h"
#include "routing/BundleRouter.h"

AdminRegistration::AdminRegistration()
    : Registration(ADMIN_REGID,
                   BundleRouter::local_tuple_,
                   DEFER)
{
    logpathf("/reg/admin");

    if (! RegistrationTable::instance()->add(this)) {
        log_err("unexpected error adding registration to table");
    }
}

void
AdminRegistration::consume_bundle(Bundle* bundle)
{
    const char* buf;
    char typecode;
    
    size_t payload_len = bundle->payload_.length();
    log_debug("got %d byte bundle", payload_len);

    if (payload_len == 0) {
        log_err("admin registration got 0 byte bundle *%p", bundle);
        goto done;
    }

    /*
     * As outlined in the bundle specification, the first byte of all
     * administrative bundles is a type code, with the following
     * values:
     *
     * 0x1     - bundle status report
     * 0x2     - custodial signal
     * 0x3     - ping request (tbd)
     * (other) - reserved
     */
    buf = bundle->payload_.read_data(0, 1);
    typecode = *buf;

    switch(typecode) {
    case BundleProtocol::STATUS_REPORT:
        PANIC("status report not implemented yet");
        break;

    case BundleProtocol::CUSTODY_SIGNAL:
        PANIC("custody signal not implemented yet");
        break;

    case BundleProtocol::PING:
        log_info("ping from %s", bundle->replyto_.c_str());
        break;
        
    default:
        log_warn("unexpected admin bundle with type 0x%x *%p",
                 typecode, bundle);
    }    

 done:
    BundleForwarder::post(
        new BundleTransmittedEvent(bundle, this, payload_len, true));
}

                   

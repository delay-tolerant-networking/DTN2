
#include <oasys/util/StringBuffer.h>

#include "AdminRegistration.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleProtocol.h"
#include "routing/BundleRouter.h"

AdminRegistration::AdminRegistration()
    : Registration(ADMIN_REGID, BundleRouter::local_tuple_, ABORT)
{
    logpathf("/reg/admin");
}

void
AdminRegistration::enqueue_bundle(Bundle* bundle,
                                  const BundleMapping* mapping)
{
    char typecode;
    
    size_t payload_len = bundle->payload_.length();
    StringBuffer payload_buf(payload_len);
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
     * 0x3     - echo request
     * 0x4     - null request
     * (other) - reserved
     */
    typecode = *(bundle->payload_.read_data(0, 1, payload_buf.data()));

    switch(typecode) {
    case BundleProtocol::ADMIN_STATUS_REPORT:
        PANIC("status report not implemented yet");
        break;

    case BundleProtocol::ADMIN_CUSTODY_SIGNAL:
        PANIC("custody signal not implemented yet");
        break;

    case BundleProtocol::ADMIN_ECHO:
        log_info("ADMIN_ECHO from %s", bundle->source_.c_str());

        // XXX/demmer implement the echo
        break;
        
    case BundleProtocol::ADMIN_NULL:
        log_info("ADMIN_NULL from %s", bundle->source_.c_str());
        break;
        
    default:
        log_warn("unexpected admin bundle with type 0x%x *%p",
                 typecode, bundle);
    }    

 done:
    BundleForwarder::post(
        new BundleTransmittedEvent(bundle, this, payload_len, true));
}

                   
/**
 * Attempt to remove the given bundle from the queue.
 *
 * @return true if the bundle was dequeued, false if not.
 */
bool
AdminRegistration::dequeue_bundle(Bundle* bundle,
                                  BundleMapping** mappingp)
{
    // since there's no queue, we can't ever dequeue something
    return false;
}


/**
 * Check if the given bundle is already queued on this consumer.
 */
bool
AdminRegistration::is_queued(Bundle* bundle)
{
    return false;
}

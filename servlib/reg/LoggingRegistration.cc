
#include <oasys/util/StringUtils.h>

#include "LoggingRegistration.h"
#include "Registration.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleForwarder.h"
#include "bundling/BundleList.h"

LoggingRegistration::LoggingRegistration(const BundleTuplePattern& endpoint)
    : Registration(endpoint, Registration::ABORT)
{
    logpathf("/registration/logging/%d", regid_);
    set_active(true);
    
    log_info("new logging registration on endpoint %s", endpoint.c_str());
}

void
LoggingRegistration::run()
{
    Bundle* b;
    while (1) {
        b = bundle_list_->pop_blocking();

        log_info("BUNDLE: id %d priority %s dopts: [%s %s %s %s %s]",
                 b->bundleid_, Bundle::prioritytoa(b->priority_),
                 b->custreq_ ? "custreq" : "",
                 b->custody_rcpt_ ? "custody_rcpt" : "",
                 b->recv_rcpt_ ? "recv_rcpt" : "",
                 b->fwd_rcpt_ ? "fwd_rcpt" : "",
                 b->return_rcpt_ ? "return_rcpt" : "");

        log_info("        source:    %s", b->source_.c_str());
        log_info("        dest:      %s", b->dest_.c_str());
        log_info("        replyto:   %s", b->replyto_.c_str());
        log_info("        custodian: %s", b->custodian_.c_str());
        log_info("        created %u.%u, expiration %d",
                 (u_int32_t)b->creation_ts_.tv_sec,
                 (u_int32_t)b->creation_ts_.tv_usec,
                 b->expiration_);

        size_t len = 128;
        size_t payload_len = b->payload_.length();
        if (payload_len < len) {
            len = payload_len;
        }

        u_char payload_buf[payload_len];
        const u_char* data = b->payload_.read_data(0, len, payload_buf);

	if (str_isascii(data, len)) {
            log_info("        payload (ascii): length %d '%.*s'",
                     payload_len, (int)len, data);
        } else {
            std::string hex;
            hex2str(&hex, data, len);
            len *= 2;
            if (len > 128)
                len = 128;
            log_info("        payload (binary): length %d %.*s",
                     payload_len, (int)len, hex.data());
        }

        BundleForwarder::post(
            new BundleTransmittedEvent(b, this, b->payload_.length(), true));
        
        b->del_ref("LoggingRegistration");
    }
}


#include "LoggingRegistration.h"
#include "Registration.h"
#include "RegistrationTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleList.h"
#include "storage/GlobalStore.h"
#include "util/StringUtils.h"

LoggingRegistration::LoggingRegistration(const BundleTuple& endpoint)
{
    u_int32_t regid = GlobalStore::instance()->next_regid();
    logpathf("/registration/logging/%d", regid);
    registration_ = new Registration(regid, endpoint, Registration::ABORT);
    registration_->set_active(true);
    
    if (! RegistrationTable::instance()->add(registration_)) {
        log_err("unexpected error adding registration to table");
    }

    log_info("new logging registration on endpoint %s", endpoint.c_str());
}

void
LoggingRegistration::run()
{
    Bundle* b;
    BundleList* blist = registration_->bundle_list();
    while (1) {
        b = blist->pop_blocking();

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
        if (b->payload_.length() < len) {
            len = b->payload_.length();
        }

        const u_char* data = (const u_char*)b->payload_.data();
        if (str_isascii(data, len)) {
            log_info("        payload (ascii): '%.*s'", len, data);
        } else {
            std::string hex;
            hex2str(&hex, data, len);
            len *= 2;
            if (len > 128)
                len = 128;
            log_info("        payload (binary): %.*s", len, hex.data());
        }
    }
}


#include "BundleStatusReport.h"
#include "BundleProtocol.h"
#include <netinet/in.h>

BundleStatusReport::BundleStatusReport(Bundle* orig_bundle, BundleTuple& source)
    : Bundle()
{
    source_.assign(source);
    replyto_.assign(source);
    custodian_.assign(source);

    dest_.assign(orig_bundle->replyto_);

    // the payload's length is variable based on the original bundle
    // source's region and admin length
    size_t region_len = orig_bundle->source_.region().length();
    size_t admin_len =  orig_bundle->source_.admin().length();
    
    size_t len = sizeof(BundleStatusReport) + 2 + region_len + admin_len;

    payload_.set_length(len, BundlePayload::MEMORY);
    ASSERT(payload_.location() == BundlePayload::MEMORY);
    
    StatusReport* report = (StatusReport*)payload_.memory_data();
    memset(report, 0, sizeof(StatusReport));

    report->admin_type = ADMIN_STATUS_REPORT;
    report->orig_ts = ((u_int64_t)htonl(orig_bundle->creation_ts_.tv_sec)) << 32;
    report->orig_ts |= (u_int64_t)htonl(orig_bundle->creation_ts_.tv_usec);
    
    char* bp = &report->tuple_data[0];
    *bp = region_len;
    memcpy(&bp[1], orig_bundle->source_.region().data(), region_len);
    bp += 1 + region_len;
 
    *bp = admin_len;
    memcpy(&bp[1], orig_bundle->source_.admin().data(), admin_len);
    bp += 1 + admin_len;
}

/**
 * Sets the appropriate status report flag and timestamp to the
 * current time.
 */
void
BundleStatusReport::set_status_time(status_report_flag_t flag)
{
    StatusReport* report = (StatusReport*)payload_.memory_data();

    u_int64_t* ts;
    
    switch(flag) {
    case STATUS_RECEIVED:
        ts = &report->receipt_ts;
        break;
            
    case STATUS_CUSTODY_ACCEPTED:
        ts = &report->receipt_ts;
        break;
                
    case STATUS_FORWARDED:
        ts = &report->forward_ts;
        break;
                    
    case STATUS_DELIVERED:
        ts = &report->delivery_ts;
        break;
        
    case STATUS_TTL_EXPIRED:
        ts = &report->delete_ts;
        break;
                            
    case STATUS_UNAUTHENTIC:
    case STATUS_UNUSED:
    case STATUS_FRAGMENT:
        NOTIMPLEMENTED;
    }

    struct timeval now;
    gettimeofday(&now, 0);

    report->status_flags |= flag;
    *ts = ((u_int64_t)htonl(now.tv_sec)) << 32;
    *ts	|= (u_int64_t)htonl(now.tv_usec);
}

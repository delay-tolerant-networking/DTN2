#ifndef _BUNDLE_STATUS_REPORT_H_
#define _BUNDLE_STATUS_REPORT_H_

#include "Bundle.h"
#include "BundleProtocol.h"

/**
 * A special bundle class whose payload is a status report. Used at
 * the source to generate the report that is then sent to the 
 */
class BundleStatusReport : public Bundle, private BundleProtocol {
public:
    BundleStatusReport(Bundle* orig_bundle, BundleTuple& source);

    /**
     * Sets the appropriate status report flag and timestamp to the
     * current time.
     */
    void set_status_time(status_report_flag_t flag);
};

#endif /* _BUNDLE_STATUS_REPORT_H_ */

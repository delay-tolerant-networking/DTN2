#ifndef _RATE_ESTIMATOR_REG_H_
#define _RATE_ESTIMATOR_REG_H_

#include "reg/Registration.h"
#include "thread/Timer.h"
#include "util/RateEstimator.h"

/**
 * Simple registration that periodically logs the rate of bundle
 * consumption.
 */
class RateEstimatorRegistration : public Registration, public RateEstimator {
public:
    RateEstimatorRegistration(const BundleTuplePattern& endpoint,
                              int interval);
    virtual ~RateEstimatorRegistration();

    void consume_bundle(Bundle* bundle);
    void timeout(struct timeval* now);

protected:
    RateEstimator* estimator_;
    int total_bytes_;
    int log_interval_;
};

#endif /* _RATE_ESTIMATOR_REG_H_ */

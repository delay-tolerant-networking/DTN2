#include "RateEstimatorRegistration.h"
#include "bundling/Bundle.h"
#include "debug/Log.h"
#include "util/RateEstimator.h"

RateEstimatorRegistration::
RateEstimatorRegistration(const BundleTuplePattern& endpoint,
                          int interval)
    : Registration(endpoint, Registration::ABORT),
      RateEstimator(&total_bytes_, interval),
      total_bytes_(0)
{
}

RateEstimatorRegistration::~RateEstimatorRegistration()
{
    delete estimator_;
}

void
RateEstimatorRegistration::timeout(struct timeval* now)
{
    RateEstimator::timeout(now);
    
    logf("/rate", LOG_INFO, "%s: %6f bps", endpoint_.c_str(), rate());
}

void
RateEstimatorRegistration::consume_bundle(Bundle* bundle)
{
    total_bytes_ += bundle->payload_.length();
}

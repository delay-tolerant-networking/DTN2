#include "RateEstimator.h"

RateEstimator::RateEstimator(int *var, int interval, double weight)
{
    var_ = var;
    weight_ = weight;
    lastval_ = 0;
    rate_ = 0.0;
    lastts_.tv_sec = 0;
    lastts_.tv_usec = 0;

    schedule_in(interval);
}

void
RateEstimator::timeout(struct timeval* now)
{
    if (lastts_.tv_sec == 0 && lastts_.tv_usec == 0) {
        // first time through
        rate_    = 0.0;
 done:
        lastval_ = *var_;
        lastts_  = *now;
        schedule_in(interval_);
        return;
    }
    
    double dv = (double)(*var_ - lastval_);
    double dt = TIMEVAL_DIFF_DOUBLE(*now, lastts_);
    
    double newrate = dv/dt;
    double oldrate = rate_;
    double delta   = newrate - oldrate;

    rate_ += delta * weight_;

    goto done;
}

#ifndef _RATE_ESTIMATOR_H_
#define _RATE_ESTIMATOR_H_

#include "thread/Timer.h"

/**
 * Simple rate estimation class that does a weighted filter of
 * samples.
 */
 
class RateEstimator : public Timer {
public:
    RateEstimator(int *var, int interval, double weight = 0.125);
    double rate() { return rate_; }
    virtual void timeout(struct timeval* now);
    
protected:
    int* var_;		///< variable being estimated
    double rate_;	///< the estimated rate
    int lastval_;	///< last sample value
    int interval_;	///< timer interval (ms)
    timeval lastts_;	///< last sample timestamp
    double weight_;	///< weighting factor for sample decay
};

#endif /* _RATE_ESTIMATOR_H_ */

#ifndef _LOGGING_REGISTRATION_H_
#define _LOGGING_REGISTRATION_H_

#include "debug/Log.h"
#include "bundling/BundleTuple.h"
#include "thread/Thread.h"

class Registration;

/**
 * A simple utility class used mostly for testing registrations.
 *
 * When created, this sets up a new registration within the daemon,
 * and for any bundles that arrive, outputs logs of the bundle header
 * fields as well as the payload data (if ascii). The implementation
 * is structured as a thread that blocks (forever) waiting for bundles
 * to arrive on the registration's bundle list, then logging the
 * bundles and looping again.
 */
class LoggingRegistration : public Thread, public Logger {
public:
    LoggingRegistration(const BundleTuple& endpoint);
    virtual void run();
    
protected:
    Registration* registration_;
};

#endif /* _LOGGING_REGISTRATION_H_ */

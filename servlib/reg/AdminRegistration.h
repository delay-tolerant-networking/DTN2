#ifndef _ADMIN_REGISTRATION_H_
#define _ADMIN_REGISTRATION_H_

#include "Registration.h"

/**
 * Internal registration that recieves all administrative bundles
 * destined for the router itself (i.e. status reports, custody
 * acknowledgements, ping bundles, etc.)
*/
class AdminRegistration : public Registration {
public:
    AdminRegistration();
    void consume_bundle(Bundle* bundle);
};

#endif /* _ADMIN_REGISTRATION_H_ */

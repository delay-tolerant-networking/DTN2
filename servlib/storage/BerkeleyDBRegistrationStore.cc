#ifndef __DB_DISABLED__

#include "BerkeleyDBRegistrationStore.h"

/**
 * Constructor
 */
BerkeleyDBRegistrationStore::BerkeleyDBRegistrationStore()
{
}

/**
 * Destructor
 */
BerkeleyDBRegistrationStore::~BerkeleyDBRegistrationStore()
{
    NOTREACHED;
}

/**
 * Load in the whole database of registrations, populating the
 * given list.
 */
void
BerkeleyDBRegistrationStore::load(RegistrationList* reg_list)
{
    //NOTIMPLEMENTED;
}

/**
 * Add a new registration to the database. Returns true if the
 * registration is successfully added, false on error.
 */
bool
BerkeleyDBRegistrationStore::add(Registration* reg)
{
    //NOTIMPLEMENTED;
    return true;
}

    
/**
 * Remove the registration from the database, returns true if
 * successful, false on error.
 */
bool
BerkeleyDBRegistrationStore::del(Registration* reg)
{
    NOTIMPLEMENTED;
}
    
/**
 * Update the registration in the database. Returns true on
 * success, false if there's no matching registration or on error.
 */
bool
BerkeleyDBRegistrationStore::update(Registration* reg)
{
    NOTIMPLEMENTED;
}

#endif /* __DB_DISABLED__ */

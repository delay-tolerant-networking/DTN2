#ifndef _BERKELEY_DB_REGISTRATION_STORE_H_
#define _BERKELEY_DB_REGISTRATION_STORE_H_

#include "RegistrationStore.h"

class BerkeleyDBStore;

/**
 * Implementation of RegistrationStore that uses an underlying
 * BerkeleyDB database.
 */
class BerkeleyDBRegistrationStore : public RegistrationStore {
public:
    /**
     * Constructor
     */
    BerkeleyDBRegistrationStore();

    /**
     * Destructor
     */
    virtual ~BerkeleyDBRegistrationStore();

    /**
     * Load in the whole database of registrations, populating the
     * given list.
     */
    virtual void load(RegistrationList* reg_list);

    /**
     * Add a new registration to the database. Returns true if the
     * registration is successfully added, false on error.
     */
    virtual bool add(Registration* reg);
    
    /**
     * Remove the registration from the database, returns true if
     * successful, false on error.
     */
    virtual bool del(Registration* reg);
    
    /**
     * Update the registration in the database. Returns true on
     * success, false if there's no matching registration or on error.
     */
    virtual bool update(Registration* reg);

protected:
    BerkeleyDBStore* store_;
};

#endif /* _BERKELEY_DB_REGISTRATION_STORE_H_ */

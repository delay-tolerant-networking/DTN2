#ifndef _SQL_REGISTRATION_STORE_H_
#define _SQL_REGISTRATION_STORE_H_

#include "RegistrationStore.h"

class SQLStore;
class SQLImplementation;

/**
 * Implementation of RegistrationStore that uses an underlying SQL database.
 */
class SQLRegistrationStore : public RegistrationStore {
public:
    /**
     * Constructor
     */
    SQLRegistrationStore(SQLImplementation* impl);

    /**
     * Destructor
     */
    virtual ~SQLRegistrationStore();

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
    SQLStore* store_;
};

#endif /* _SQL_REGISTRATION_STORE_H_ */

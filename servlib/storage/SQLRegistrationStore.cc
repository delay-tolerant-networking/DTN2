
#include "SQLRegistrationStore.h"
#include "SQLStore.h"
#include "SQLImplementation.h"

static const char* TABLENAME = "registration";

/**
 * Constructor
 */
SQLRegistrationStore::SQLRegistrationStore(SQLImplementation* impl)
{
    store_ = new SQLStore(TABLENAME, impl);
}

/**
 * Destructor
 */
SQLRegistrationStore::~SQLRegistrationStore()
{
    NOTREACHED;
}

/**
 * Load in the whole database of registrations, populating the
 * given list.
 */
void
SQLRegistrationStore::load(RegistrationList* reg_list)
{
    // NOTIMPLEMENTED
}

/**
 * Add a new registration to the database. Returns true if the
 * registration is successfully added, false on error.
 */
bool
SQLRegistrationStore::add(Registration* reg)
{
    NOTIMPLEMENTED;
}

/**
 * Remove the registration from the database, returns true if
 * successful, false on error.
 */
bool
SQLRegistrationStore::del(u_int32_t regid, const std::string& endpoint)
{
    NOTIMPLEMENTED;
}
    
/**
 * Update the registration in the database. Returns true on
 * success, false if there's no matching registration or on error.
 */
bool
SQLRegistrationStore::update(Registration* reg)
{
    NOTIMPLEMENTED;
}


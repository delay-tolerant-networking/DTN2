#ifndef dtn_registration_store_h
#define dtn_registration_store_h

#include <vector>

class Bundle;
class Registration;

typedef std::vector<Registration*> RegistrationList;

/**
 * Abstract base class for registration storage.
 */
class RegistrationStore {
public:
    RegistrationStore();
    
    /// @{ Basic storage methods
    virtual Registration* get(int reg_id) = 0;
    virtual int           put(Registration* reg) = 0;
    virtual int           del(int reg_id) = 0;
    /// @}
    
    /**
     * Get a new registration id, updating the value in the persistent
     * store.
     *
     * (was db_new_regID, db_update_registration_id, db_restore_registration_id)
     */
    virtual int next_id() = 0;

    /**
     * Delete any expired registrations
     *
     * (was sweepOldRegistrations)
     */
    virtual int delete_expired(const time_t now) = 0;

    /**
     * Fill in the RegistrationList with any registrations matching
     * the given bundle
     *
     * (was db_get_matching_registrations)
     */
    virtual int get_matching(const Bundle* bundle, RegistrationList* reg_list) = 0;

    /*
     * Dump out the registration database.
     */
    virtual void dump(FILE* fp) = 0;

private:
    int next_reg_id_; // running serial number for bundles
};

#endif /* dtn_registration_store_h */

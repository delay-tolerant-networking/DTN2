#ifndef _REGISTRATION_STORE_H_
#define _REGISTRATION_STORE_H_

#include <vector>
#include "debug/Debug.h"

class Bundle;
class Registration;

typedef std::vector<Registration*> RegistrationList;

/**
 * Abstract base class for registration storage.
 */
class RegistrationStore {
public:
    /**
     * Singleton instance accessor.
     */
    static RegistrationStore* instance() {
        ASSERT(instance_ != NULL);
        return instance_;
    }

    /**
     * Boot time initializer that takes as a parameter the actual
     * instance to use.
     */
    static void init(RegistrationStore* instance) {
        ASSERT(instance_ == NULL);
        instance_ = instance;
    }
    
    /**
     * Constructor
     */
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
    static RegistrationStore* instance_;
    
    int next_reg_id_; // running serial number for bundles
};

#endif /* _REGISTRATION_STORE_H_ */

#ifndef _REGISTRATION_TABLE_H_
#define _REGISTRATION_TABLE_H_

#include <string>
#include "Registration.h"
#include "debug/Debug.h"

class RegistrationStore;

/**
 * Class for the in-memory registration table. All changes to the
 * table are made persistent via the abstract RegistrationStore
 * interface.
 */
class RegistrationTable : public Logger {
public:
    /**
     * Singleton instance accessor.
     */
    static RegistrationTable* instance() {
        if (instance_ == NULL) {
            PANIC("RegistrationTable::init not called yet");
        }
        return instance_;
    }

    /**
     * Boot time initializer that takes as a parameter the actual
     * storage instance to use.
     */
    static void init(RegistrationStore* store) {
        if (instance_ != NULL) {
            PANIC("RegistrationTable::init called multiple times");
        }
        instance_ = new RegistrationTable(store);
    }

    /**
     * Constructor
     */
    RegistrationTable(RegistrationStore* store);

    /**
     * Destructor
     */
    virtual ~RegistrationTable();

    /**
     * Load in the registration table.
     */
    bool load();

    /**
     * Add a new registration to the database. Returns true if the
     * registration is successfully added, false if there's another
     * registration with the same {endpoint,regid}.
     */
    bool add(Registration* reg);
    
    /**
     * Look up a matching registration.
     */
    Registration* get(u_int32_t regid, const BundleTuple& endpoint);

    /**
     * Remove the registration from the database, returns true if
     * successful, false if the registration didn't exist.
     */
    bool del(u_int32_t regid, const BundleTuple& endpoint);
    
    /**
     * Update the registration in the database. Returns true on
     * success, false on error.
     */
    bool update(Registration* reg);
    
    /**
     * Populate the given reglist with all registrations with an
     * endpoint id that matches the bundle demux string.
     *
     * Returns the count of matching registrations.
     */
    int get_matching(const BundleTuple& tuple, RegistrationList* reg_list);
    
    /**
     * Delete any expired registrations
     *
     * (was sweepOldRegistrations)
     */
    int delete_expired(const time_t now);

    /*
     * Dump out the registration database.
     */
    void dump(FILE* fp);

protected:
    static RegistrationTable* instance_;
    
    /**
     * Internal method to find the location of the given registration.
     */
    bool find(u_int32_t regid,
              const BundleTuple& endpoint,
              RegistrationList::iterator* iter);
    
    /**
     * All registrations are tabled in-memory in a flat list. It's
     * non-obvious what else would be better since we need to do a
     * prefix match on demux strings in matching_registrations.
     */
    RegistrationList reglist_;
};

#endif /* _REGISTRATION_TABLE_H_ */

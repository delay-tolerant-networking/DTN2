#ifndef _REGISTRATION_STORE_H_
#define _REGISTRATION_STORE_H_

#include <string>
#include "reg/Registration.h"

class PersistentStore;

/**
 * Abstract base class for the persistent registration store.
 */
class RegistrationStore : public Logger {
public:
    /**
     * Singleton instance accessor.
     */
    static RegistrationStore* instance() {
        if (instance_ == NULL) {
            PANIC("RegistrationStore::init not called yet");
        }
        return instance_;
    }

    /**
     * Boot time initializer that takes as a parameter the actual
     * instance to use.
     */
    static void init(RegistrationStore* instance) {
        if (instance_ != NULL) {
            PANIC("RegistrationStore::init called multiple times");
        }
        instance_ = instance;
    }

    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance_ != NULL); }
    
    /**
     * Constructor
     */
    RegistrationStore(PersistentStore * store);
    
    /**
     * Destructor
     */
    ~RegistrationStore();

    /**
     * Load stored registrations from database into system
     */
    bool load();

    /**
     * Load in the whole database of registrations, populating the
     * given list.
     */
    void load(RegistrationList* reg_list);

    /**
     * Add a new registration to the database. Returns true if the
     * registration is successfully added, false on error.
     */
    bool add(Registration* reg);
    
    /**
     * Remove the registration from the database, returns true if
     * successful, false on error.
     */
    bool del(Registration* reg);
    
    /**
     * Update the registration in the database. Returns true on
     * success, false if there's no matching registration or on error.
     */
    bool update(Registration* reg);

protected:
    static RegistrationStore* instance_;

    PersistentStore * store_;
};

#endif /* _REGISTRATION_STORE_H_ */

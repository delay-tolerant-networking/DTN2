#ifndef _CONTACT_H_
#define _CONTACT_H_

class BundleList;
class ContactInfo;

#include "debug/Debug.h"
#include "debug/Formatter.h"
#include "bundling/BundleTuple.h"

/**
 * Valid types for a contact.
 */
typedef enum
{
    /**
     * The contact is expected to be either ALWAYS available, or can
     * be made available easily. Examples include DSL (always), and
     * dialup (easily available).
     */
    ONDEMAND = 1,
    
    /**
     * The contact is only available at pre-determined times.
     */
    SCHEDULED = 2,

    /**
     * The contact may or may not be available, based on
     * uncontrollable factors. Examples include a wireless link whose
     * connectivity depends on the relative locations of the two
     * nodes.
     */
    OPPORTUNISTIC = 3
}
contact_type_t;

/**
 * Contact type string conversion.
 */
inline const char*
contact_type_toa(contact_type_t type)
{
    switch(type) {
    case ONDEMAND: 	return "ONDEMAND";
    case SCHEDULED: 	return "SCHEDULED";
    case OPPORTUNISTIC: return "OPPORTUNISTIC";
    default: 		PANIC("bogus contact_type_t");
    }
}

/**
 * Encapsulation of a connection to a next-hop DTN contact. The object
 * contains a list of bundles that are destined for it, as well as a
 * slot to store any convergence layer specific attributes.
 */
class Contact : public Formatter, public Logger {
public:
    /**
     * Constructor / Destructor
     */
    Contact(contact_type_t type, const BundleTuple& tuple);
    virtual ~Contact();
    
    /**
     * Accessor for the list of bundles in this contact.
     */
    BundleList* bundle_list() { return bundle_list_; }
    
    /**
     * Store the convergence layer specific part of the contact.
     */
    void set_contact_info(ContactInfo* contact_info)
    {
        ASSERT(contact_info_ == NULL);
        contact_info_ = contact_info;
    }
    
    ContactInfo* contact_info() { return contact_info_; }

    // virtual from Formatter
    int format(char* buf, size_t sz);

protected:
    contact_type_t type_;
    BundleTuple tuple_;
    
    ContactInfo* contact_info_;
    BundleList* bundle_list_;
};

/**
 * Abstract base class for convergence layer specific portions of a
 * contact.
 */
class ContactInfo {
public:
    virtual ~ContactInfo();
};

#endif /* _CONTACT_H_ */

#ifndef _CONTACT_H_
#define _CONTACT_H_

class Bundle;
class BundleList;
class ContactInfo;
class ConvergenceLayer;

#include <oasys/debug/Debug.h>
#include <oasys/debug/Formatter.h>

#include "BundleConsumer.h"
#include "BundleTuple.h"

//#include "debug/Debug.h"
//#include "debug/Formatter.h"
#include "Link.h"

class Link;
class ContactInfo;
class ConvergenceLayer;

/**
 * Encapsulation of a connection to a next-hop DTN contact. The object
 * contains a list of bundles that are destined for it, as well as a
 * slot to store any convergence layer specific attributes.
 */
class Contact : public Formatter, public BundleConsumer {
public:

    /**
     * Constructor / Destructor
     */
    Contact(Link* link);
    virtual ~Contact();
 
    /**
     * Queue a bundle for sending, which potentially kicks open the
     * contact if it's not already.
     */
    void enqueue_bundle(Bundle* bundle, const BundleMapping* mapping);
    
    /**
     * Accessor to this contact's convergence layer.
     */
    ConvergenceLayer* clayer();

    /**
     * Store the convergence layer specific part of the contact.
     */
    void set_contact_info(ContactInfo* contact_info)
    {
        ASSERT(contact_info_ == NULL || contact_info == NULL);
        contact_info_ = contact_info;
    }
    
    /**
     * Accessor to the contact info.
     */
    ContactInfo* contact_info() { return contact_info_; }
    
    /**
     * Accessor to the link
     */
    Link* link() { return link_; }

    /**
     * Accessor to tuple
     */
    BundleTuple tuple() { return link_->tuple() ; }

    /**
     * Virtual from formatter
     */
    int format(char* buf, size_t sz);

    /**
     * Virtual from bundle consumer
     */
    const char* type() { return "Contact" ;}
    
protected:
    Link* link_ ; ///> Pointer to parent link on which this contact exists
    ContactInfo* contact_info_; ///> convergence layer specific info

};


/**
 * Abstract base class for convergence layer specific portions of a
 * contact.
 */
class ContactInfo {
public:
    virtual ~ContactInfo() {}
};


#endif /* _CONTACT_H_ */

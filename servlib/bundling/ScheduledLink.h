#ifndef _SCHEDULED_LINK_H_
#define _SCHEDULED_LINK_H_

#include "Link.h"
#include <set>

/**
 * Abstraction for a SCHEDULED link.
 *
 * Scheduled links have a list  of future contacts.
 *
*/

class FutureContact;

class ScheduledLink : public Link {
public:

    typedef std::set<FutureContact*> FutureContactSet;

    /**
     * Constructor / Destructor
     */
    ScheduledLink(std::string name, const char* conv_layer,
         const BundleTuple& tuple);
 
    virtual ~ScheduledLink();
    
    /**
     * Return the list of future contacts that exist on the link
     */
    FutureContactSet* future_contacts() { return fcts_; }

    // Add a future contact
    void add_fc(FutureContact* fc) ;

    // Remove a future contact
    void delete_fc(FutureContact* fc) ;

    // Convert a future contact to an active contact
    void convert_fc(FutureContact* fc) ;

    // Return list of all future contacts
    FutureContactSet* future_contacts_list() { return fcts_; }
    
protected:
    FutureContactSet*  fcts_;
};

/**
 * Abstract base class for FutureContact
 * Relevant only for scheduled links.
 */
class FutureContact : public BundleConsumer {
public:
    /**
     * Constructor / Destructor
     */
    FutureContact(const BundleTuple& tuple) :
        BundleConsumer(&tuple,false), start_(0), duration_(0) {
         logpathf("/fc_contact/%s", tuple.c_str());
         bundle_list_ = new BundleList(logpath_);
         log_debug("new future contact *%p", this);
    }
    virtual ~FutureContact()  {
        ASSERT(bundle_list_->size() == 0);
        delete bundle_list_;
    }
    /**
     * Virtual from bundle consumer
     */
    const char* type() { return "FC" ;}
  
protected:
    // Time at which contact starts, 0 value means not defined
    time_t start_;
    // Duration for this future contact, 0 value means not defined
    time_t duration_;
};


#endif /* _SCHEDULED_LINK_H_ */

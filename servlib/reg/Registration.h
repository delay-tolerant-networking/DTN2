#ifndef _REGISTRATION_H_
#define _REGISTRATION_H_

#include <list>
#include <string>
#include "bundling/BundleConsumer.h"
#include "bundling/BundleTuple.h"
#include "debug/Log.h"
#include "storage/Serialize.h"

class Bundle;
class BundleList;

/**
 * Class used to represent an "application" registration, loosly
 * defined to also include internal router mechanisms that consume
 * bundles.
 *
 * Stored in the RegistrationTable, indexed by key of
 * {registration_id,endpoint}.
 *
 * Registration state is stored persistently in the database.
 */
class Registration : public BundleConsumer, public SerializableObject,
                     public Logger {
public:
    /**
     * Reserved registration identifiers.
     */
    static const u_int32_t ADMIN_REGID = 0;
    
    /**
     * Type enumerating the option requested by the registration for
     * how to handle bundles when not connected.
     */
    typedef enum {
        DEFER,		///< Spool bundles until requested
        ABORT,		///< Drop bundles
        EXEC		///< Execute the specified callback procedure
    } failure_action_t;

    /**
     * Constructor that allocates a new registration id.
     */
    Registration(const BundleTuplePattern& endpoint,
                 failure_action_t action,
                 time_t expiration = 0,
                 const std::string& script = "");
    
    /**
     * Constructor with a preassigned registration id.
     */
    Registration(u_int32_t regid,
                 const BundleTuplePattern& endpoint,
                 failure_action_t action,
                 time_t expiration = 0,
                 const std::string& script = "");
    
    /**
     * Destructor.
     */
    virtual ~Registration();
    
    //@{
    /// Accessors
    u_int32_t			regid()		{ return regid_; }
    const BundleTuplePattern&	endpoint() 	{ return endpoint_; } 
    failure_action_t		failure_action(){ return failure_action_; }
    //@}

    /**
     * Accessor indicating whether or not the registration is
     * currently expecting bundles, as defined as a period of "passive
     * bundle activity" in the bundle spec.
     */
    bool active() { return active_; }

    /**
     * Accessor to set the active state.
     */
    void set_active(bool active) { active_ = active; }
    
    /**
     * Accessor for the list of bundles to be delivered to this
     * registration. 
     */
    BundleList* bundle_list() { return bundle_list_; }

    /**
     * Consume a bundle for delivery to the application. The default
     * implementation just queues the bundle on this registration's
     * bundle list, but derived classes (e.g. AdminRegistration)
     * always immediately process bundles.
     */
    virtual void consume_bundle(Bundle* bundle);

    /**
     * Virtual from SerializableObject.
     */
    void serialize(SerializeAction* a);

protected:
    void init(u_int32_t regid,
              const BundleTuplePattern& endpoint,
              failure_action_t action,
              time_t expiration,
              const std::string& script);

    u_int32_t regid_;
    BundleTuplePattern endpoint_;
    failure_action_t failure_action_;	
    std::string script_;
    time_t expiration_;
    BundleList* bundle_list_;
    bool active_;
};

/**
 * Typedef for a list of Registrations.
 */
class RegistrationList : public std::list<Registration*> {};

#endif /* _REGISTRATION_H_ */

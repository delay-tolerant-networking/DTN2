#ifndef _REGISTRATION_H_
#define _REGISTRATION_H_

#include <list>
#include <string>

#include <oasys/debug/Log.h>
#include <oasys/serialize/Serialize.h>

#include "../bundling/BundleConsumer.h"
#include "../bundling/BundleTuple.h"

class Bundle;
class BundleList;
class BundleMapping;

class Registration;
// XXX/namespace
namespace dtn {
typedef ::Registration Registration;
};

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
class Registration : public BundleConsumer, public SerializableObject {
public:
    /**
     * Reserved registration identifiers.
     */
    static const u_int32_t ADMIN_REGID = 0;
    static const u_int32_t RESERVED_REGID = 1;
    
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
     * Default constructor for serialization
     */
    Registration(u_int32_t regid);

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
    bool active_;
};

/**
 * Typedef for a list of Registrations.
 */
class RegistrationList : public std::list<Registration*> {};

#endif /* _REGISTRATION_H_ */

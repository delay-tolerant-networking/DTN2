#ifndef _REGISTRATION_H_
#define _REGISTRATION_H_

#include <list>
#include <string>
#include "debug/Log.h"
#include "storage/Serialize.h"

class Bundle;
class BundleList;

/**
 * Class used to represent an application registration. Stored in the
 * RegistrationTable, indexed by key of {demux,registration_id}.
 *
 * Registration state is stored persistently in the database.
 */
class Registration : public SerializableObject, public Logger {
public:
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
     * Constructor.
     */
    Registration(u_int32_t regid, const std::string& endpoint,
                 failure_action_t action, const std::string& script = "",
                 time_t expiration = 0);
    
    /**
     * Destructor.
     */
    virtual ~Registration();
    
    //@{
    /// Accessors
    u_int32_t		regid()		{ return regid_; }
    const std::string&	endpoint() 	{ return endpoint_; } 
    failure_action_t	failure_action(){ return failure_action_; }
    //@}

    /**
     * Accessor indicating whether or not the registration is
     * currently "active", i.e. whether to execute the failure action
     * or not.
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
     * Queue a bundle for delivery to the application.
     */
    void consume_bundle(Bundle* bundle);

    /**
     * Virtual from SerializableObject.
     */
    void serialize(SerializeAction* a);

protected:
    u_int32_t regid_;
    std::string endpoint_;
    failure_action_t failure_action_;	
    std::string script_;
    time_t expiration_;
    BundleList* bundle_list_;
    bool active_;
};

/**
 * Typedef for a list of Registrations.
 */
typedef std::list<Registration*> RegistrationList;

#endif /* _REGISTRATION_H_ */

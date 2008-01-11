/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _REGISTRATION_H_
#define _REGISTRATION_H_

#include <list>
#include <string>

#include <oasys/debug/Log.h>
#include <oasys/serialize/Serialize.h>
#include <oasys/thread/Timer.h>

#include "../naming/EndpointID.h"

namespace dtn {

class Bundle;

/**
 * Class used to represent an "application" registration, loosely
 * defined to also include internal router mechanisms that consume
 * bundles.
 *
 * Stored in the RegistrationTable, indexed by regid.
 *
 * Registration state is stored persistently in the database.
 */
class Registration : public oasys::SerializableObject, public oasys::Logger {
public:
    /**
     * Reserved registration identifiers.
     */
    static const u_int32_t ADMIN_REGID = 0;
    static const u_int32_t LINKSTATEROUTER_REGID = 1;
    static const u_int32_t PING_REGID = 2;
    static const u_int32_t EXTERNALROUTER_REGID = 3;
    static const u_int32_t DTLSR_REGID = 4;
    static const u_int32_t MAX_RESERVED_REGID = 9;
    
    /**
     * Type enumerating the option requested by the registration for
     * how to handle bundles when not connected.
     */
    typedef enum {
        DROP,		///< Drop bundles
        DEFER,		///< Spool bundles until requested
        EXEC		///< Execute the specified callback procedure
    } failure_action_t;

    /**
     * Get a string representation of a failure action.
     */
    static const char* failure_action_toa(failure_action_t action);

    /**
     * Constructor for deserialization
     */
    Registration(const oasys::Builder&);

    /**
     * Constructor.
     */
    Registration(u_int32_t regid,
                 const EndpointIDPattern& endpoint,
                 u_int32_t failure_action,
                 u_int32_t session_flags,
                 u_int32_t expiration,
                 const std::string& script = "");

    /**
     * Destructor.
     */
    virtual ~Registration();

    /**
     * Abstract hook for subclasses to deliver a bundle to this registration.
     */
    virtual void deliver_bundle(Bundle* bundle) = 0;
    
    //@{
    /// Accessors
    u_int32_t	             durable_key()       { return regid_; }
    u_int32_t	             regid()	         { return regid_; }
    const EndpointIDPattern& endpoint()          { return endpoint_; } 
    failure_action_t         failure_action()
    {
        return static_cast<failure_action_t>(failure_action_);
    }
    u_int32_t                session_flags()     { return session_flags_; }
    const std::string&       script()            { return script_; }
    u_int32_t	             expiration()	 { return expiration_; }
    bool                     active()            { return active_; }
    void                     set_active(bool a)  { active_ = a; }
    bool                     expired()           { return expired_; }
    void                     set_expired(bool e) { expired_ = e; }
    //@}

    /**
     * Virtual from SerializableObject.
     */
    void serialize(oasys::SerializeAction* a);

    /**
     * Force the registration to expire immediately. This hook is used
     * to allow applications to call unregister on a bound
     * registration, which causes the registration to be cleaned up
     * once the app quits.
     */
    void force_expire();

protected:
    /**
     * Class to implement registration expirations.
     */
    class ExpirationTimer : public oasys::Timer {
    public:
        ExpirationTimer(Registration* reg)
            : reg_(reg) {}

        void timeout(const struct timeval& now);
        
        Registration* reg_;
    };

    void init_expiration_timer();
    void cleanup_expiration_timer();
    
    u_int32_t regid_;
    EndpointIDPattern endpoint_;
    u_int32_t failure_action_;	
    u_int32_t session_flags_;	
    std::string script_;
    u_int32_t expiration_;
    u_int32_t creation_time_;
    ExpirationTimer* expiration_timer_;
    bool active_;    
    bool bound_;    
    bool expired_;    
};

/**
 * Typedef for a list of Registrations.
 */
class RegistrationList : public std::list<Registration*> {};

} // namespace dtn

#endif /* _REGISTRATION_H_ */

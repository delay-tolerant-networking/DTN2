/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _REGISTRATION_H_
#define _REGISTRATION_H_

#include <list>
#include <string>

#include <oasys/debug/Log.h>
#include <oasys/serialize/Serialize.h>

#include "../bundling/BundleConsumer.h"
#include "../bundling/BundleTuple.h"

namespace dtn {

class Bundle;
class BundleList;
class BundleMapping;

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
class Registration : public BundleConsumer, public oasys::SerializableObject {
public:
    /**
     * Reserved registration identifiers. XXX/demmer fix me
     */
    static const u_int32_t ADMIN_REGID = 0;
    static const u_int32_t RESERVED_REGID_1 = 1;
    static const u_int32_t RESERVED_REGID_2 = 2;
    static const u_int32_t MAX_RESERVED_REGID = 9;
    
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
     * Get a string representation of a failure action.
     */
    static const char* failure_action_toa(failure_action_t action);

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
    void serialize(oasys::SerializeAction* a);
 
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

} // namespace dtn

#endif /* _REGISTRATION_H_ */

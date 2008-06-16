/*
 *    Copyright 2005-2006 Intel Corporation
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

#ifndef _API_REGISTRATION_H_
#define _API_REGISTRATION_H_

#include <list>
#include "Registration.h"

namespace dtn {

class BlockingBundleList;

/**
 * Registration class to represent an actual attached application over
 * the client api.
 */
class APIRegistration : public Registration {
public:
    /**
     * Constructor for deserialization
     */
    APIRegistration(const oasys::Builder& builder);

    /**
     * Constructor.
     */
    APIRegistration(u_int32_t regid,
                    const EndpointIDPattern& endpoint,
                    failure_action_t action,
                    u_int32_t session_flags,
                    u_int32_t expiration,
                    const std::string& script = "");

    ~APIRegistration();

    /// Virtual from SerializableObject
    void serialize(oasys::SerializeAction* a);
    
    /// Virtual from Registration
    void deliver_bundle(Bundle* bundle);
    void session_notify(Bundle* bundle);
    
    /**
     * Accessor for the queue of bundles for the registration.
     */
    BlockingBundleList* bundle_list() { return bundle_list_; }

    /**
     * Accessor for notification of session subscribers /
     * unsubscribers (currently just the subscription bundles).
     */
    BlockingBundleList* session_notify_list() { return session_notify_list_; }
    
protected:
    /// Queue of bundles for the registration
    BlockingBundleList* bundle_list_;

    /// Queue of subscription notification bundles
    BlockingBundleList* session_notify_list_;
};

/**
 * Typedef for a list of APIRegistrations.
 */
class APIRegistrationList : public std::list<APIRegistration*> {};

} // namespace dtn

#endif /* _API_REGISTRATION_H_ */

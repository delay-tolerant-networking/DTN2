/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
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
                    const BundleTuplePattern& endpoint,
                    failure_action_t action,
                    time_t expiration = 0,
                    const std::string& script = "");

    ~APIRegistration();
    
    /// @{
    /// Virtual from BundleConsumer
    virtual void consume_bundle(Bundle* bundle);
    virtual bool dequeue_bundle(Bundle* bundle);
    virtual bool is_queued(Bundle* bundle);
    /// @}
    
    /**
     * Accessor for the queue of bundles for the registration.
     * XXX/demmer fixme 
     */
    BlockingBundleList* xxx_bundle_list() { return bundle_list_; }
    
protected:
    /// Queue of bundles for the registration
    BlockingBundleList* bundle_list_;	
};

/**
 * Typedef for a list of APIRegistrations.
 */
class APIRegistrationList : public std::list<APIRegistration*> {};

} // namespace dtn

#endif /* _API_REGISTRATION_H_ */

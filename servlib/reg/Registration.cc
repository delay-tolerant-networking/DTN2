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

#include "Registration.h"
#include "RegistrationTable.h"
#include "bundling/Bundle.h"
#include "bundling/BundleList.h"
#include "bundling/BundleMapping.h"
#include "storage/GlobalStore.h"

namespace dtn {

const char*
Registration::failure_action_toa(failure_action_t action)
{
    switch(action) {
    case DEFER:	return "DEFER";
    case ABORT: return "ABORT";
    case EXEC:  return "EXEC";
    }

    return "__INVALID__";
}

void
Registration::init(u_int32_t regid,
                   const BundleTuplePattern& endpoint,
                   failure_action_t action,
                   time_t expiration,
                   const std::string& script)
{
    regid_ = regid;
    endpoint_.assign(endpoint);
    failure_action_ = action;
    script_.assign(script);
    expiration_ = expiration;
    active_ = false;

    logpathf("/registration/%d", regid);
    
    bundle_list_ = new BundleList(logpath_);
    
    if (expiration == 0) {
        // XXX/demmer default expiration
    }

    if (! RegistrationTable::instance()->add(this)) {
        log_err("unexpected error adding registration to table");
    }
}

/**
 * Constructor.
 */
Registration::Registration(u_int32_t regid)
    : BundleConsumer(&endpoint_, true, "Reg")
{
    regid_ = regid;
}

/**
 * Constructor.
 */
Registration::Registration(const BundleTuplePattern& endpoint,
                           failure_action_t action,
                           time_t expiration,
                           const std::string& script)
    
    : BundleConsumer(&endpoint_, true, "Reg")
{
    init(GlobalStore::instance()->next_regid(),
         endpoint, action, expiration, script);
}

Registration::Registration(u_int32_t regid,
                           const BundleTuplePattern& endpoint,
                           failure_action_t action,
                           time_t expiration,
                           const std::string& script)
    
    : BundleConsumer(&endpoint_, true, "Reg")
{
    init(regid, endpoint, action, expiration, script);
}

/**
 * Destructor.
 */
Registration::~Registration()
{
    // XXX/demmer loop through bundle list and remove refcounts
    NOTIMPLEMENTED;
    delete bundle_list_;
}
    
/**
 * Virtual from SerializableObject.
 */
void
Registration::serialize(oasys::SerializeAction* a)
{
    a->process("endpoint", &endpoint_);
    a->process("regid", &regid_);
    a->process("action", (u_int32_t*)&failure_action_);
    a->process("script", &script_);
    a->process("expiration", (u_int32_t*)&expiration_);
}

} // namespace dtn

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

#include "RegistrationTable.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "storage/RegistrationStore.h"

namespace dtn {

RegistrationTable::RegistrationTable()
    : Logger("/registration")
{
}

RegistrationTable::~RegistrationTable()
{
    NOTREACHED;
}

/**
 * Internal method to find the location of the given registration.
 */
bool
RegistrationTable::find(u_int32_t regid, const BundleTuple& endpoint,
                        RegistrationList::iterator* iter)
{
    Registration* reg;

    for (*iter = reglist_.begin(); *iter != reglist_.end(); ++(*iter)) {
        reg = *(*iter);
        
        if ((reg->regid() == regid) &&
            (reg->endpoint().compare(endpoint) == 0))
        {
            return true;
        }
    }

    return false;
}

/**
 * Look up a matching registration.
 */
Registration*
RegistrationTable::get(u_int32_t regid, const BundleTuple& endpoint)
{
    RegistrationList::iterator iter;

    if (find(regid, endpoint, &iter)) {
        return *iter;
    }
    return NULL;
}

/**
 * Add a new registration to the database. Returns true if the
 * registration is successfully added, false if there's another
 * registration with the same {endpoint,regid}.
 */
bool
RegistrationTable::add(Registration* reg)
{
    // check if a conflicting registration already exists
    if (get(reg->regid(), reg->endpoint()) != NULL) {
        log_err("error adding registration %d/%s: duplicate registration",
                reg->regid(), reg->endpoint().c_str());
        return false; 
    }

    // put it in the list
    reglist_.push_back(reg);

    // don't store (or log) default registrations 
    if (reg->regid() <= Registration::MAX_RESERVED_REGID) {
        return true;
    }

    log_info("adding registration %d/%s", reg->regid(),
             reg->endpoint().c_str());
    
    if (! RegistrationStore::instance()->add(reg)) {
        log_err("error adding registration %d/%s: error in persistent store",
                reg->regid(), reg->endpoint().c_str());
        return false;
    }
    
    return true;
}

/**
 * Remove the registration from the database, returns true if
 * successful, false if the registration didn't exist.
 */
bool
RegistrationTable::del(u_int32_t regid, const BundleTuple& endpoint)
{
    RegistrationList::iterator iter;

    log_info("removing registration %d/%s", regid, endpoint.c_str());
    
    if (! find(regid, endpoint, &iter)) {
        log_err("error removing registration %d/%s: no matching registration",
                regid, endpoint.c_str());
        return false;
    }

    if (! RegistrationStore::instance()->del(*iter)) {
        log_err("error removing registration %d/%s: error in persistent store",
                regid, endpoint.c_str());
        return false;
        
    }

    reglist_.erase(iter);

    return true;
}
    
/**
 * Update the registration in the database. Returns true on
 * success, false on error.
 */
bool
RegistrationTable::update(Registration* reg)
{
    log_info("updating registration %d/%s",
             reg->regid(), reg->endpoint().c_str());

    if (! RegistrationStore::instance()->update(reg)) {
        log_err("error updating registration %d/%s: error in persistent store",
                reg->regid(), reg->endpoint().c_str());
        return false;
    }

    return true;
}
    
/**
 * Populate the given reglist with all registrations with an endpoint
 * id that matches the prefix of that in the bundle demux string.
 *
 * Returns the count of matching registrations.
 */
int
RegistrationTable::get_matching(const BundleTuple& demux,
                                RegistrationList* reg_list)
{
    int count = 0;
    
    RegistrationList::iterator iter;
    Registration* reg;

    log_debug("get_matching %s", demux.c_str());

    for (iter = reglist_.begin(); iter != reglist_.end(); ++iter) {
        reg = *iter;

        if (reg->endpoint().match(demux)) {
            log_debug("matched registration %d %s",
                      reg->regid(), reg->endpoint().c_str());
            count++;
            reg_list->push_back(reg);
        }
    }

    log_debug("get_matching %s: returned %d matches", demux.c_str(), count);
    return count;
}


void
RegistrationTable::dump(oasys::StringBuffer* buf) const
{
    RegistrationList::const_iterator i;
    for (i = reglist_.begin(); i != reglist_.end(); ++i)
    {
	Registration* reg = *i;
        
	buf->appendf("id %u: %s (%s)\n",
                     reg->regid(),
                     reg->endpoint().c_str(),
                     Registration::failure_action_toa((reg->failure_action())));
    }
}

} // namespace dtn

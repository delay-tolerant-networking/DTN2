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

#include "APIRegistration.h"
#include "RegistrationTable.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "storage/RegistrationStore.h"

namespace dtn {

//----------------------------------------------------------------------
RegistrationTable::RegistrationTable()
    : Logger("RegistrationTable", "/dtn/registration/table")
{
}

//----------------------------------------------------------------------
RegistrationTable::~RegistrationTable()
{
    while (! reglist_.empty()) {
        delete reglist_.front();
        reglist_.pop_front();
    }
}

//----------------------------------------------------------------------
bool
RegistrationTable::find(u_int32_t regid, RegistrationList::iterator* iter)
{
    Registration* reg;

    for (*iter = reglist_.begin(); *iter != reglist_.end(); ++(*iter)) {
        reg = *(*iter);

        if (reg->regid() == regid) {
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------
Registration*
RegistrationTable::get(u_int32_t regid) const
{
    RegistrationList::iterator iter;

    // the const_cast lets us use the same find method for get as we
    // use for add/del
    if (const_cast<RegistrationTable*>(this)->find(regid, &iter)) {
        return *iter;
    }
    return NULL;
}

//----------------------------------------------------------------------
Registration*
RegistrationTable::get(const EndpointIDPattern& eid) const
{
    Registration* reg;
    RegistrationList::const_iterator iter;
    
    for (iter = reglist_.begin(); iter != reglist_.end(); ++iter) {
        reg = *iter;
        if (reg->endpoint().equals(eid)) {
            return reg;
        }
    }
    
    return NULL;
}

//----------------------------------------------------------------------
bool
RegistrationTable::add(Registration* reg, bool add_to_store)
{
    // put it in the list
    reglist_.push_back(reg);

    // don't store (or log) default registrations 
    if (!add_to_store || reg->regid() <= Registration::MAX_RESERVED_REGID) {
        return true;
    }

    // now, all we should get are API registrations
    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(reg);
    if (api_reg == NULL) {
        log_err("non-api registration %d passed to registration store",
                reg->regid());
        return false;
    }
    
    log_info("adding registration %d/%s", reg->regid(),
             reg->endpoint().c_str());
    
    if (! RegistrationStore::instance()->add(api_reg)) {
        log_err("error adding registration %d/%s: error in persistent store",
                reg->regid(), reg->endpoint().c_str());
        return false;
    }
    
    return true;
}

//----------------------------------------------------------------------
bool
RegistrationTable::del(u_int32_t regid)
{
    RegistrationList::iterator iter;

    log_info("removing registration %d", regid);
    
    if (! find(regid, &iter)) {
        log_err("error removing registration %d: no matching registration",
                regid);
        return false;
    }

    if (! RegistrationStore::instance()->del(regid)) {
        log_err("error removing registration %d: error in persistent store",
                regid);
        return false;
    }

    reglist_.erase(iter);

    return true;
}

//----------------------------------------------------------------------
bool
RegistrationTable::update(Registration* reg)
{
    log_info("updating registration %d/%s",
             reg->regid(), reg->endpoint().c_str());

    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(reg);
    if (api_reg == NULL) {
        log_err("non-api registration %d passed to registration store",
                reg->regid());
        return false;
    }
    
    if (! RegistrationStore::instance()->update(api_reg)) {
        log_err("error updating registration %d/%s: error in persistent store",
                reg->regid(), reg->endpoint().c_str());
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
int
RegistrationTable::get_matching(const EndpointID& demux,
                                RegistrationList* reg_list) const
{
    int count = 0;
    
    RegistrationList::const_iterator iter;
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

        buf->appendf("id %u: %s %s (%s%s) [expiration %d]\n",
                     reg->regid(),
                     reg->active() ? "active" : "passive",
                     reg->endpoint().c_str(),
                     Registration::failure_action_toa(reg->failure_action()),
                     reg->failure_action() == Registration::EXEC ?
                        reg->script().c_str() : "",
                     reg->expiration());
    }
}

} // namespace dtn

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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

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
    oasys::ScopeLock l(&lock_, "RegistrationTable");

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
Registration*
RegistrationTable::get(const EndpointIDPattern& eid, u_int64_t reg_token) const
{
    Registration* reg;
    RegistrationList::const_iterator iter;
    

    for (iter = reglist_.begin(); iter != reglist_.end(); ++iter) {
        reg = *iter;
        if (reg->endpoint().equals(eid)) {
            APIRegistration* api_reg = dynamic_cast<APIRegistration*>(reg);
            if (api_reg == NULL) {
                continue;
            }
            if ( api_reg->reg_token()==reg_token ) {
                return reg;
            }
        }
    }
    
    return NULL;
}

//----------------------------------------------------------------------
bool
RegistrationTable::add(Registration* reg, bool add_to_store)
{
    oasys::ScopeLock l(&lock_, "RegistrationTable");

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
    oasys::ScopeLock l(&lock_, "RegistrationTable");

    RegistrationList::iterator iter;

    log_info("removing registration %d", regid);
    
    if (! find(regid, &iter)) {
        log_err("error removing registration %d: no matching registration",
                regid);
        return false;
    }

    reglist_.erase(iter);

    // Store (or log) default registrations and not pushed to persistent store
    if (regid <= Registration::MAX_RESERVED_REGID) {
        return true;
    }

    if (! RegistrationStore::instance()->del(regid)) {
        log_err("error removing registration %d: error in persistent store",
                regid);
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
bool
RegistrationTable::update(Registration* reg) const
{
    oasys::ScopeLock l(&lock_, "RegistrationTable");

    log_debug("updating registration %d/%s",
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
    oasys::ScopeLock l(&lock_, "RegistrationTable");

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
    oasys::ScopeLock l(&lock_, "RegistrationTable");

    RegistrationList::const_iterator i;
    for (i = reglist_.begin(); i != reglist_.end(); ++i)
    {
        buf->appendf("*%p\n", *i);
    }
}

/**
 * Return the routing table.  Asserts that the RegistrationTable
 * spin lock is held by the caller.
 */
const RegistrationList *
RegistrationTable::reg_list() const
{
    ASSERTF(lock_.is_locked_by_me(),
            "RegistrationTable::reg_list must be called while holding lock");
    return &reglist_;
}

} // namespace dtn

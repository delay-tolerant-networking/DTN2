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
#include "reg/Registration.h"
#include "reg/RegistrationTable.h"
#include "RegistrationStore.h"
#include "PersistentStore.h"

namespace dtn {

RegistrationStore* RegistrationStore::instance_;

/**
 * Constructor
 */
RegistrationStore::RegistrationStore()
    : Logger("RegistrationStore", "/dtn/storage/registration")
{
}

int
RegistrationStore::do_init(const oasys::StorageConfig& cfg,
                           oasys::DurableStore*        store)
{
#ifdef __REG_STORE_ENABLED__
    NOTREACHED; // XXX/demmer implement me
#endif
    return 0;
}

/**
 * Destructor
 */
RegistrationStore::~RegistrationStore()
{
    NOTREACHED;
}

/**
 * Load in the database of registrations.
 */
void
RegistrationStore::load()
{
#ifdef __REG_STORE_ENABLED__ 
    std::vector<int> ids;
    std::vector<int>::iterator iter;
    store_->keys(&ids);

    for (iter = ids.begin();
         iter != ids.end();
         ++iter)
    {
        int id = *iter;
        Registration * reg = new APIRegistration(id);
        if (store_->get(reg, id) == 0)
        {
            reg_list->push_back(reg);
        }
    }
#endif /* __REG_STORE_ENABLED__ */
    
}

/**
 * Add a new registration to the database. Returns true if the
 * registration is successfully added, false on error.
 */
bool
RegistrationStore::add(Registration* reg)
{
#ifdef __REG_STORE_ENABLED__
    // ignore reserved registration ids
    if (reg->regid() < 10)
    {
        return true;
    }
    else
    {   
        return store_->add(reg, reg->regid()) == 0;
    }
#else /* __REG_STORE_ENABLED__ */
    return true;
#endif /* __REG_STORE_ENABLED__ */
}

/**
 * Remove the registration from the database, returns true if
 * successful, false on error.
 */
bool
RegistrationStore::del(Registration* reg)
{
#ifdef __REG_STORE_ENABLED__
    return store_->del(reg->regid()) == 0;
#else /* __REG_STORE_ENABLED__ */
    return true;
#endif
}
    
/**
 * Update the registration in the database. Returns true on
 * success, false if there's no matching registration or on error.
 */
bool
RegistrationStore::update(Registration* reg)
{
#ifdef __REG_STORE_ENABLED__ 
    return store_->update(reg, reg->regid()) == 0;
#else /* __REG_STORE_ENABLED__ */
    return true;
#endif /* __REG_STORE_ENABLED__ */
}

void
RegistrationStore::close()
{
#ifdef __REG_STORE_ENABLED__
    log_debug("closing registration store");
    delete store_
    if (store_->close() != 0) {
        log_err("error closing registration store");
    }
#endif
}

} // namespace dtn

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

#include <oasys/storage/DurableStore.h>
#include <oasys/storage/StorageConfig.h>
#include <oasys/serialize/TypeShims.h>

#include "RegistrationStore.h"
#include "reg/APIRegistration.h"

namespace dtn {

RegistrationStore* RegistrationStore::instance_;

static const char* REG_TABLE = "registrations";

//----------------------------------------------------------------------
RegistrationStore::RegistrationStore()
    : Logger("RegistrationStore", "/dtn/storage/registration")
{
}

//----------------------------------------------------------------------
int
RegistrationStore::do_init(const oasys::StorageConfig& cfg,
                           oasys::DurableStore*        store)
{
    int flags = 0;

    if (cfg.init_)
        flags |= oasys::DS_CREATE;
    
    int err = store->get_table(&store_, REG_TABLE, flags);

    if (err != 0) {
        log_err("error initializing registration store");
        return err;
    }

    return 0;
}

//----------------------------------------------------------------------
RegistrationStore::~RegistrationStore()
{
    NOTREACHED;
}

//----------------------------------------------------------------------
bool
RegistrationStore::add(APIRegistration* reg)
{
    // ignore reserved registration ids
    if (reg->regid() < 10)
    {
        return true;
    }

    int err = store_->put(oasys::UIntShim(reg->regid()), reg,
                          oasys::DS_CREATE | oasys::DS_EXCL);
    
    if (err == oasys::DS_EXISTS) {
        log_err("add registration %d: already exists", reg->regid());
        return false;
    }

    if (err != 0) {
        PANIC("RegistrationStore::add(%d): fatal database error", reg->regid());
    }

    return true;
}

//----------------------------------------------------------------------
bool
RegistrationStore::del(u_int32_t reg_id)
{
    int err = store_->del(oasys::UIntShim(reg_id));
    if (err != 0) {
        log_err("del registration %d: %s", reg_id,
                (err == oasys::DS_NOTFOUND) ?
                "registration doesn't exist" : "unknown error");
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
APIRegistration*
RegistrationStore::get(u_int32_t reg_id)
{
    APIRegistration* reg = NULL;
    int err = store_->get(oasys::UIntShim(reg_id), &reg);
    if (err == oasys::DS_NOTFOUND) {
        log_warn("RegistrationStore::get(%d): reg doesn't exist", reg_id);
        return NULL;
    }

    ASSERTF(err == 0,
            "RegistrationStore::get(%d): internal database error", reg_id);

    ASSERT(reg != NULL);
    ASSERT(reg->regid() == reg_id);
    return reg;
}

//----------------------------------------------------------------------
bool
RegistrationStore::update(APIRegistration* reg)
{
    int err = store_->put(oasys::UIntShim(reg->regid()), reg, 0);

    if (err == oasys::DS_NOTFOUND) {
        log_err("update registration %d: reg doesn't exist", reg->regid());
        return false;
    }
    
    if (err != 0) {
        PANIC("RegStore::update(%d): fatal database error", reg->regid());
    }

    return true;
}

//----------------------------------------------------------------------
void
RegistrationStore::close()
{
    log_debug("closing registration store");

    delete store_;
    store_ = NULL;
}

//----------------------------------------------------------------------
RegistrationStore::iterator*
RegistrationStore::new_iterator()
{
    return new RegistrationStore::iterator(store_->itr());
}

//----------------------------------------------------------------------
RegistrationStore::iterator::iterator(oasys::DurableIterator* iter)
    : iter_(iter), cur_regid_(0)
{
}

//----------------------------------------------------------------------
RegistrationStore::iterator::~iterator()
{
    delete iter_;
    iter_ = NULL;
}

//----------------------------------------------------------------------
int
RegistrationStore::iterator::next()
{
    int err = iter_->next();
    if (err == oasys::DS_NOTFOUND)
    {
        return err; // all done
    }
    
    else if (err != oasys::DS_OK)
    {
        __log_err(RegistrationStore::instance()->logpath(),
                  "error in iterator next");
        return err;
    }

    oasys::UIntShim key;
    err = iter_->get_key(&key);
    if (err != 0)
    {
        __log_err(RegistrationStore::instance()->logpath(),
                  "error in iterator get");
        return oasys::DS_ERR;
    }
    
    cur_regid_ = key.value();
    return oasys::DS_OK;
}

} // namespace dtn

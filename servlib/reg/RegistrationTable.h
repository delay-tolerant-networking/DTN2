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
#ifndef _REGISTRATION_TABLE_H_
#define _REGISTRATION_TABLE_H_

#include <string>
#include <oasys/debug/Debug.h>
#include <oasys/util/StringBuffer.h>

#include "Registration.h"

namespace dtn {

class RegistrationStore;

/**
 * Class for the in-memory registration table. All changes to the
 * table are made persistent via the abstract RegistrationStore
 * interface.
 */
class RegistrationTable : public oasys::Logger {
public:
    /**
     * Singleton instance accessor.
     */
    static RegistrationTable* instance() {
        if (instance_ == NULL) {
            PANIC("RegistrationTable::init not called yet");
        }
        return instance_;
    }

    /**
     * Boot time initializer that takes as a parameter the actual
     * storage instance to use.
     */
    static void init(RegistrationStore* store) {
        if (instance_ != NULL) {
            PANIC("RegistrationTable::init called multiple times");
        }
        instance_ = new RegistrationTable(store);
    }

    /**
     * Constructor
     */
    RegistrationTable(RegistrationStore* store);

    /**
     * Destructor
     */
    virtual ~RegistrationTable();

    /**
     * Load in the registration table.
     */
    bool load();

    /**
     * Add a new registration to the database. Returns true if the
     * registration is successfully added, false if there's another
     * registration with the same {endpoint,regid}.
     */
    bool add(Registration* reg);
    
    /**
     * Look up a matching registration.
     */
    Registration* get(u_int32_t regid, const BundleTuple& endpoint);

    /**
     * Remove the registration from the database, returns true if
     * successful, false if the registration didn't exist.
     */
    bool del(u_int32_t regid, const BundleTuple& endpoint);
    
    /**
     * Update the registration in the database. Returns true on
     * success, false on error.
     */
    bool update(Registration* reg);
    
    /**
     * Populate the given reglist with all registrations with an
     * endpoint id that matches the bundle demux string.
     *
     * Returns the count of matching registrations.
     */
    int get_matching(const BundleTuple& tuple, RegistrationList* reg_list);
    
    /**
     * Delete any expired registrations
     *
     * (was sweepOldRegistrations)
     */
    int delete_expired(const time_t now);

    /**
     * Dump out the registration database.
     */
    void dump(oasys::StringBuffer* buf) const;

protected:
    static RegistrationTable* instance_;
    
    /**
     * Internal method to find the location of the given registration.
     */
    bool find(u_int32_t regid,
              const BundleTuple& endpoint,
              RegistrationList::iterator* iter);
    
    /**
     * All registrations are tabled in-memory in a flat list. It's
     * non-obvious what else would be better since we need to do a
     * prefix match on demux strings in matching_registrations.
     */
    RegistrationList reglist_;
};

} // namespace dtn

#endif /* _REGISTRATION_TABLE_H_ */

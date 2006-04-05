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
#ifndef _REGISTRATION_STORE_H_
#define _REGISTRATION_STORE_H_

#include <oasys/debug/Log.h>

// forward decl
namespace oasys {
template<typename _Type> class SingleTypeDurableTable;
class StorageConfig;
class DurableIterator;
class DurableStore;
}

namespace dtn {

class APIRegistration;

/**
 * The persistent store for API registrations.
 */
class RegistrationStore : public oasys::Logger {
public:
    /**
     * Singleton instance accessor.
     */
    static RegistrationStore* instance() {
        if (instance_ == NULL) {
            PANIC("RegistrationStore::init not called yet");
        }
        return instance_;
    }

    /**
     * Boot time initializer that takes as a parameter the actual
     * instance to use.
     */
    static int init(const oasys::StorageConfig& cfg,
                    oasys::DurableStore*        store) 
    {
        if (instance_ != NULL) {
            PANIC("RegistrationStore::init called multiple times");
        }
        instance_ = new RegistrationStore();
        return instance_->do_init(cfg, store);
    }

    /**
     * Constructor.
     */
    RegistrationStore();

    /**
     * Real initialization method.
     */
    int do_init(const oasys::StorageConfig& cfg,
                oasys::DurableStore*        store);

    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance_ != NULL); }
    
    /**
     * Destructor
     */
    ~RegistrationStore();

    /**
     * Load in the database of registrations.
     */
    void load();

    /**
     * Close (and flush) the data store.
     */
    void close();

    /**
     * Add a new registration to the database. Returns true if the
     * registration is successfully added, false on error.
     */
    bool add(APIRegistration* reg);
    
    /**
     * Load a registration from the store.
     */
    APIRegistration* get(u_int32_t regid);
    
    /**
     * Remove the registration from the database, returns true if
     * successful, false on error.
     */
    bool del(u_int32_t regid);
    
    /**
     * Update the registration in the database. Returns true on
     * success, false if there's no matching registration or on error.
     */
    bool update(APIRegistration* reg);

    /**
     * Iterator class used to loop through the keys (i.e. bundle ids)
     * in the store.
     */
    class iterator {
    public:
        /**
         * Destructor that cleans up the internal iterator as well.
         */
        virtual ~iterator();

        /**
         * Advances the iterator.
         *
         * @return DS_OK, DS_NOTFOUND if no more elements, DS_ERR if
         * an error occurred while iterating.
         */
        int next();

        /**
         * Return the registration id at the current location.
         */
        u_int32_t cur_regid() { return cur_regid_; }

    private:
        friend class RegistrationStore;

        /// Private constructor, used only by the BundleStore.
        iterator(oasys::DurableIterator* iter);

        /// The underlying iterator
        oasys::DurableIterator* iter_;

        /// Cache of the current registration id
        u_int32_t cur_regid_;
    };

    /**
     * Return a new iterator.
     */
    iterator* new_iterator();
    
protected:
    static RegistrationStore* instance_;
    oasys::SingleTypeDurableTable<APIRegistration>* store_;
};
} // namespace dtn

#endif /* _REGISTRATION_STORE_H_ */

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

#include <oasys/debug/Debug.h>
#include <oasys/thread/SpinLock.h>

#include "Bundle.h"
#include "BundleList.h"
#include "BundleMapping.h"
#include "storage/GlobalStore.h"

namespace dtn {

void
Bundle::init(u_int32_t id)
{
    bundleid_		= id;
    refcount_		= 0;
    pendingtxcount_     = 0;
    priority_		= COS_NORMAL;
    expiration_		= 0;
    custreq_		= false;
    custody_rcpt_	= false;
    recv_rcpt_		= false;
    fwd_rcpt_		= false;
    return_rcpt_	= false;
    expiration_		= 0; // XXX/demmer
    gettimeofday(&creation_ts_, 0);

    is_fragment_	= false;
    is_reactive_fragment_ = false;
    orig_length_	= 0;
    frag_offset_	= 0;

    log_debug("/bundle", "Bundle::init bundle id %d", id);
}

Bundle::Bundle()
{
    u_int32_t id = GlobalStore::instance()->next_bundleid();
    init(id);
    payload_.init(&lock_, id);
}

Bundle::Bundle(u_int32_t id, BundleStore* store)
{
    init(id);
    payload_.init(&lock_, id, store);
}

Bundle::Bundle(u_int32_t id, BundlePayload::location_t location)
{
    init(id);
    payload_.init(&lock_, id, location);
}

Bundle::~Bundle()
{
    ASSERT(mappings_.size() == 0);
    // XXX/demmer remove the bundle from the database
    
    bundleid_ = 0xdeadf00d;
}

int
Bundle::format(char* buf, size_t sz)
{
    return snprintf(buf, sz, "bundle id %d %s -> %s (%d bytes payload)",
                    bundleid_, source_.c_str(), dest_.c_str(),
                    (u_int32_t)payload_.length());
}

void
Bundle::serialize(oasys::SerializeAction* a)
{
    a->process("bundleid", &bundleid_);
    a->process("source", &source_);
    a->process("dest", &dest_);
    a->process("custodian", &custodian_);
    a->process("replyto", &replyto_);
    a->process("priority", &priority_);
    a->process("custreq", &custreq_);
    a->process("custody_rcpt", &custody_rcpt_);
    a->process("recv_rcpt", &recv_rcpt_);
    a->process("fwd_rcpt", &fwd_rcpt_);
    a->process("return_rcpt", &return_rcpt_);
    a->process("creation_ts_sec",  (u_int32_t*)&creation_ts_.tv_sec);
    a->process("creation_ts_usec", (u_int32_t*)&creation_ts_.tv_usec);
    a->process("expiration", &expiration_);
    a->process("payload", &payload_);
    a->process("is_fragment", &is_fragment_);
    a->process("orig_length", &orig_length_);
    a->process("frag_offset", &frag_offset_);
}

/**
 * Bump up the reference count.
 */
int
Bundle::add_ref(const char* what1, const char* what2)
{
    lock_.lock();
    ASSERT(refcount_ >= 0);
    int ret = ++refcount_;
    log_debug("/bundle/refs",
              "bundle id %d: refcount %d -> %d (%d mappings) add %s %s",
              bundleid_, refcount_ - 1, refcount_, mappings_.size(),
              what1, what2);
    lock_.unlock();
    return ret;
}

/**
 * Decrement the reference count.
 *
 * If the reference count becomes zero, the bundle is deleted.
 */
int
Bundle::del_ref(const char* what1, const char* what2)
{
    lock_.lock();
    ASSERT(refcount_ > 0);
    int ret = --refcount_;
    log_debug("/bundle/refs",
              "bundle id %d: refcount %d -> %d (%d mappings) del %s %s",
              bundleid_, refcount_ + 1, refcount_, mappings_.size(),
              what1, what2);
    
    if (refcount_ != 0) {
        lock_.unlock();
        return ret;
    }

    log_debug("/bundle", "bundle id %d: no more references, deleting bundle",
              bundleid_);
    delete this;
    return 0;
}

/**
 * Bump up the pending transmission count
 */
int
Bundle::add_pending()
{
    lock_.lock();
    ASSERT(pendingtxcount_ >= 0);
    int ret = ++pendingtxcount_;
    log_debug("/bundle/pending",
              "bundle id %d: add pendingtxcount %d -> %d",
              bundleid_, pendingtxcount_ - 1, pendingtxcount_);
    lock_.unlock();
    return ret;
}

/**
 * Bump down the pending transmission count
 */
int
Bundle::del_pending()
{
    lock_.lock();
    ASSERT(pendingtxcount_ > 0);
    int ret = --pendingtxcount_;
    log_debug("/bundle/pending",
              "bundle id %d: del pendingtxcount %d -> %d",
              bundleid_, pendingtxcount_ + 1, pendingtxcount_);
    lock_.unlock();
    return ret;
}

/**
 * Add a BundleList to the set of mappings.
 *
 * @return true if the mapping was added successfully, false if it
 * was already in the set
 */
BundleMapping*
Bundle::add_mapping(BundleList* blist, const BundleMapping* mapping_info)
{
    oasys::ScopeLock l(&lock_);
    
    log_debug("/bundle/mapping", "bundle id %d add mapping [%s]",
              bundleid_, blist->name().c_str());

    BundleMapping* mapping = mapping_info ?
                             new BundleMapping(*mapping_info) :
                             new BundleMapping();
    
    BundleMappings::value_type val(blist, mapping);
                                   
    if (mappings_.insert(val).second == true) {
        return mapping;
    }
    
    log_err("/bundle/mapping", "ERROR in add mapping: "
            "bundle id %d already on list [%s]",
            bundleid_, blist->name().c_str());

    delete mapping;
    return NULL;
}

/**
 * Remove a mapping.
 *
 * @return the mapping if it was removed successfully, NULL if
 * wasn't in the set.
 */
BundleMapping* 
Bundle::del_mapping(BundleList* blist)
{
    oasys::ScopeLock l(&lock_);

    log_debug("/bundle/mapping", "bundle id %d del mapping [%s]",
              bundleid_, blist->name().c_str());

    BundleMappings::iterator iter = mappings_.find(blist);

    if (iter == mappings_.end()) {
        log_err("/bundle/mapping", "ERROR in del mapping: "
                "bundle id %d not on list [%s]",
                bundleid_, blist->name().c_str());
        return NULL;
    }

    BundleMappings::value_type val = *iter;
    mappings_.erase(iter);
    
    return val.second;
}    
    
/**
 * Get the mapping state for the given list. Returns NULL if the
 * bundle is not currently queued on the list.
 */
BundleMapping*
Bundle::get_mapping(BundleList* blist)
{
    oasys::ScopeLock l(&lock_);

    BundleMappings::iterator iter = mappings_.find(blist);
    if (iter == mappings_.end()) {
        return NULL;
    }

    return iter->second;
}


/**
 * Return an iterator to scan through the mappings.
 */
Bundle::MappingsIterator
Bundle::mappings_begin()
{
    if (!lock_.is_locked_by_me())
        PANIC("Must lock Bundle before using mappings iterator");
    
    return mappings_.begin();
}
    
/**
 * Return an iterator to mark the end of the mappings set.
 */
Bundle::MappingsIterator
Bundle::mappings_end()
{
    if (!lock_.is_locked_by_me())
        PANIC("Must lock Bundle before using mappings iterator");
    
    return mappings_.end();
}

} // namespace dtn

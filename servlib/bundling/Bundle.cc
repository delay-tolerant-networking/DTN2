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

#include <oasys/debug/DebugUtils.h>
#include <oasys/thread/SpinLock.h>

#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleList.h"
#include "ExpirationTimer.h"

#include "storage/GlobalStore.h"

namespace dtn {

void
Bundle::init(u_int32_t id)
{
    bundleid_		= id;
    refcount_		= 0;
    
    is_fragment_	= false;
    is_admin_		= false;
    do_not_fragment_	= false;
    is_reactive_fragment_ = false;
    custody_requested_	= false;
    priority_		= COS_NORMAL;
    receive_rcpt_	= false;
    custody_rcpt_	= false;
    forward_rcpt_	= false;
    delivery_rcpt_	= false;
    deletion_rcpt_	= false;
    expiration_		= 0;
    gettimeofday(&creation_ts_, 0);
    orig_length_	= 0;
    frag_offset_	= 0;
    expiration_		= 0;
    owner_              = "";

    log_debug("/bundle", "Bundle::init bundle id %d", id);
}

Bundle::Bundle()
{
    u_int32_t id = GlobalStore::instance()->next_bundleid();
    init(id);
    payload_.init(&lock_, id);
    expiration_timer_ = NULL;
}

Bundle::Bundle(const oasys::Builder&)
{
    // don't do anything here except set the id to a bogus default
    // value and make sure the expiration timer is NULL, since the
    // fields should all be set when loaded from the database
    bundleid_ = 0xffffffff;
    expiration_timer_ = NULL;
}

Bundle::Bundle(u_int32_t id, BundlePayload::location_t location)
{
    init(id);
    payload_.init(&lock_, id, location);
    expiration_timer_ = NULL;
}

Bundle::~Bundle()
{
    ASSERT(mappings_.size() == 0);
    bundleid_ = 0xdeadf00d;

    ASSERTF(expiration_timer_ == NULL,
            "bundle deleted with pending expiration timer");
}

int
Bundle::format(char* buf, size_t sz) const
{
    return snprintf(buf, sz, "bundle id %d %s -> %s (%d bytes payload)",
                    bundleid_, source_.c_str(), dest_.c_str(),
                    (u_int32_t)payload_.length());
}

void
Bundle::format_verbose(oasys::StringBuffer* buf)
{

#define bool_to_str(x)   ((x) ? "true" : "false")

    buf->appendf("bundle id %d:\n", bundleid_);
    buf->appendf("            source: %s\n", source_.c_str());
    buf->appendf("              dest: %s\n", dest_.c_str());
    buf->appendf("         custodian: %s\n", custodian_.c_str());
    buf->appendf("           replyto: %s\n", replyto_.c_str());
    buf->appendf("    payload_length: %u\n", (u_int)payload_.length());
    buf->appendf("          priority: %d\n", priority_);
    buf->appendf(" custody_requested: %s\n", bool_to_str(custody_requested_));
    buf->appendf("      receive_rcpt: %s\n", bool_to_str(receive_rcpt_));
    buf->appendf("      custody_rcpt: %s\n", bool_to_str(custody_rcpt_));
    buf->appendf("      forward_rcpt: %s\n", bool_to_str(forward_rcpt_));
    buf->appendf("     delivery_rcpt: %s\n", bool_to_str(delivery_rcpt_));
    buf->appendf("     deletion_rcpt: %s\n", bool_to_str(deletion_rcpt_));
    buf->appendf("       creation_ts: %u.%u\n",
                 (u_int)creation_ts_.tv_sec, (u_int)creation_ts_.tv_usec);
    buf->appendf("        expiration: %d\n", expiration_);
    buf->appendf("       is_fragment: %s\n", bool_to_str(is_fragment_));
    buf->appendf("          is_admin: %s\n", bool_to_str(is_admin_));
    buf->appendf("   do_not_fragment: %s\n", bool_to_str(do_not_fragment_));
    buf->appendf("  is_reactive_frag: %s\n",
                                    bool_to_str(is_reactive_fragment_));
    buf->appendf("       orig_length: %d\n", orig_length_);
    buf->appendf("       frag_offset: %d\n", frag_offset_);
}

void
Bundle::serialize(oasys::SerializeAction* a)
{
    a->process("bundleid", &bundleid_);
    a->process("is_fragment", &is_fragment_);
    a->process("is_admin", &is_admin_);
    a->process("do_not_fragment", &do_not_fragment_);
    a->process("source", &source_);
    a->process("dest", &dest_);
    a->process("custodian", &custodian_);
    a->process("replyto", &replyto_);
    a->process("priority", &priority_);
    a->process("custody_requested", &custody_requested_);
    a->process("custody_rcpt", &custody_rcpt_);
    a->process("receive_rcpt", &receive_rcpt_);
    a->process("forward_rcpt", &forward_rcpt_);
    a->process("delivery_rcpt", &delivery_rcpt_);
    a->process("deletion_rcpt", &deletion_rcpt_);
    a->process("creation_ts_sec",  (u_int32_t*)&creation_ts_.tv_sec);
    a->process("creation_ts_usec", (u_int32_t*)&creation_ts_.tv_usec);
    a->process("expiration", &expiration_);
    a->process("payload", &payload_);
    a->process("orig_length", &orig_length_);
    a->process("frag_offset", &frag_offset_);
    a->process("owner", &owner_);
}

/**
 * Bump up the reference count.
 */
int
Bundle::add_ref(const char* what1, const char* what2)
{
    lock_.lock("Bundle::add_ref");
    ASSERT(refcount_ >= 0);
    int ret = ++refcount_;
    log_debug("/bundle/refs",
              "bundle id %d: refcount %d -> %d (%u mappings) add %s %s",
              bundleid_, refcount_ - 1, refcount_, (u_int)mappings_.size(),
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
    lock_.lock("Bundle::del_ref");
    ASSERT(refcount_ > 0);
    int ret = --refcount_;
    log_debug("/bundle/refs",
              "bundle id %d: refcount %d -> %d (%u mappings) del %s %s",
              bundleid_, refcount_ + 1, refcount_, (u_int)mappings_.size(),
              what1, what2);
    
    if (refcount_ != 0) {
        lock_.unlock();
        return ret;
    }

    log_debug("/bundle",
              "bundle id %d: no more references, posting free event",
              bundleid_);

    BundleDaemon::instance()->post(new BundleFreeEvent(this));
    
    return 0;
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

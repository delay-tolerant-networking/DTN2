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

//----------------------------------------------------------------------
void
Bundle::init(u_int32_t id)
{
    bundleid_		= id;
    is_fragment_	= false;
    is_admin_		= false;
    do_not_fragment_	= false;
    in_datastore_       = false;
    custody_requested_	= false;
    local_custody_      = false;
    singleton_dest_     = true;
    priority_		= COS_NORMAL;
    receive_rcpt_	= false;
    custody_rcpt_	= false;
    forward_rcpt_	= false;
    delivery_rcpt_	= false;
    deletion_rcpt_	= false;
    app_acked_rcpt_	= false;
    orig_length_	= 0;
    frag_offset_	= 0;
    expiration_		= 0;
    owner_              = "";

    // as per the spec, the creation timestamp should be calculated as
    // seconds since 1/1/1970, and since the bundle id should be
    // monotonically increasing, it's safe to use that for the seqno
    creation_ts_.seconds_ = BundleTimestamp::get_current_time();
    creation_ts_.seqno_   = bundleid_;

    log_debug("/dtn/bundle", "Bundle::init bundle id %d", id);
}

//----------------------------------------------------------------------
Bundle::Bundle()
    : payload_(&lock_), fwdlog_(&lock_)
{
    u_int32_t id = GlobalStore::instance()->next_bundleid();
    init(id);
    payload_.init(id);
    refcount_	      = 0;
    expiration_timer_ = NULL;
    freed_	      = false;
}

//----------------------------------------------------------------------
Bundle::Bundle(const oasys::Builder&)
    : payload_(&lock_), fwdlog_(&lock_)
{
    // don't do anything here except set the id to a bogus default
    // value and make sure the expiration timer is NULL, since the
    // fields are set and the payload initialized when loading from
    // the database
    refcount_	      = 0;
    bundleid_ 	      = 0xffffffff;
    expiration_timer_ = NULL;
    freed_	      = false;
}

//----------------------------------------------------------------------
Bundle::~Bundle()
{
    ASSERT(mappings_.size() == 0);
    bundleid_ = 0xdeadf00d;

    ASSERTF(expiration_timer_ == NULL,
            "bundle deleted with pending expiration timer");

}

//----------------------------------------------------------------------
int
Bundle::format(char* buf, size_t sz) const
{
    return snprintf(buf, sz, "bundle id %d %s -> %s (%s%s%d bytes payload)",
                    bundleid_, source_.c_str(), dest_.c_str(),
                    is_admin_    ? "is_admin " : "",
                    is_fragment_ ? "is_fragment " : "",
                    (u_int32_t)payload_.length());
}

//----------------------------------------------------------------------
void
Bundle::format_verbose(oasys::StringBuffer* buf)
{

#define bool_to_str(x)   ((x) ? "true" : "false")

    buf->appendf("bundle id %d:\n", bundleid_);
    buf->appendf("            source: %s\n", source_.c_str());
    buf->appendf("              dest: %s\n", dest_.c_str());
    buf->appendf("         custodian: %s\n", custodian_.c_str());
    buf->appendf("           replyto: %s\n", replyto_.c_str());
    buf->appendf("    payload_length: %zu\n", payload_.length());
    buf->appendf("          priority: %d\n", priority_);
    buf->appendf(" custody_requested: %s\n", bool_to_str(custody_requested_));
    buf->appendf("     local_custody: %s\n", bool_to_str(local_custody_));
    buf->appendf("    singleton_dest: %s\n", bool_to_str(singleton_dest_));
    buf->appendf("      receive_rcpt: %s\n", bool_to_str(receive_rcpt_));
    buf->appendf("      custody_rcpt: %s\n", bool_to_str(custody_rcpt_));
    buf->appendf("      forward_rcpt: %s\n", bool_to_str(forward_rcpt_));
    buf->appendf("     delivery_rcpt: %s\n", bool_to_str(delivery_rcpt_));
    buf->appendf("     deletion_rcpt: %s\n", bool_to_str(deletion_rcpt_));
    buf->appendf("    app_acked_rcpt: %s\n", bool_to_str(app_acked_rcpt_));
    buf->appendf("       creation_ts: %u.%u\n",
                 creation_ts_.seconds_, creation_ts_.seqno_);
    buf->appendf("        expiration: %d\n", expiration_);
    buf->appendf("       is_fragment: %s\n", bool_to_str(is_fragment_));
    buf->appendf("          is_admin: %s\n", bool_to_str(is_admin_));
    buf->appendf("   do_not_fragment: %s\n", bool_to_str(do_not_fragment_));
    buf->appendf("       orig_length: %d\n", orig_length_);
    buf->appendf("       frag_offset: %d\n", frag_offset_);
    buf->appendf("transmission_count: %zu\n", fwdlog_.get_transmission_count());
}

//----------------------------------------------------------------------
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
    a->process("local_custody", &local_custody_);
    a->process("singleton_dest", &singleton_dest_);
    a->process("custody_rcpt", &custody_rcpt_);
    a->process("receive_rcpt", &receive_rcpt_);
    a->process("forward_rcpt", &forward_rcpt_);
    a->process("delivery_rcpt", &delivery_rcpt_);
    a->process("deletion_rcpt", &deletion_rcpt_);
    a->process("app_acked_rcpt", &app_acked_rcpt_);
    a->process("creation_ts_seconds", &creation_ts_.seconds_);
    a->process("creation_ts_seqno", &creation_ts_.seqno_);
    a->process("expiration", &expiration_);
    a->process("payload", &payload_);
    a->process("orig_length", &orig_length_);
    a->process("frag_offset", &frag_offset_);
    a->process("owner", &owner_);

    // XXX/TODO serialize the forwarding log and make sure it's
    // updated on disk as it changes in memory
    //a->process("forwarding_log", &fwdlog_);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        in_datastore_ = true;
        payload_.init_from_store(bundleid_);
    }
}
    
//----------------------------------------------------------------------
void
Bundle::copy_metadata(Bundle* new_bundle)
{
    new_bundle->is_admin_ 		= is_admin_;
    new_bundle->is_fragment_ 		= is_fragment_;
    new_bundle->do_not_fragment_ 	= do_not_fragment_;
    new_bundle->source_ 		= source_;
    new_bundle->dest_ 			= dest_;
    new_bundle->custodian_		= custodian_;
    new_bundle->replyto_ 		= replyto_;
    new_bundle->priority_ 		= priority_;
    new_bundle->custody_requested_	= custody_requested_;
    new_bundle->local_custody_		= false;
    new_bundle->singleton_dest_		= singleton_dest_;
    new_bundle->custody_rcpt_ 		= custody_rcpt_;
    new_bundle->receive_rcpt_ 		= receive_rcpt_;
    new_bundle->forward_rcpt_ 		= forward_rcpt_;
    new_bundle->delivery_rcpt_ 		= delivery_rcpt_;
    new_bundle->deletion_rcpt_	 	= deletion_rcpt_;
    new_bundle->app_acked_rcpt_	 	= app_acked_rcpt_;
    new_bundle->creation_ts_ 		= creation_ts_;
    new_bundle->expiration_ 		= expiration_;
}

//----------------------------------------------------------------------
int
Bundle::add_ref(const char* what1, const char* what2)
{
    (void)what1;
    (void)what2;
    
    oasys::ScopeLock l(&lock_, "Bundle::add_ref");

    ASSERTF(freed_ == false, "Bundle::add_ref on bundle %d (%p)"
            "called when bundle is already being freed!", bundleid_, this);
    
    ASSERT(refcount_ >= 0);
    int ret = ++refcount_;
    log_debug("/dtn/bundle/refs",
              "bundle id %d (%p): refcount %d -> %d (%zu mappings) add %s %s",
              bundleid_, this, refcount_ - 1, refcount_,
              mappings_.size(), what1, what2);
    return ret;
}

//----------------------------------------------------------------------
int
Bundle::del_ref(const char* what1, const char* what2)
{
    (void)what1;
    (void)what2;
    
    oasys::ScopeLock l(&lock_, "Bundle::del_ref");

    ASSERTF(freed_ == false, "Bundle::del_ref on bundle %d (%p)"
            "called when bundle is already being freed!", bundleid_, this);
    
    int ret = --refcount_;
    log_debug("/dtn/bundle/refs",
              "bundle id %d (%p): refcount %d -> %d (%zu mappings) del %s %s",
              bundleid_, this, refcount_ + 1, refcount_,
              mappings_.size(), what1, what2);
    
    if (refcount_ != 0) {
        return ret;
    }

    freed_ = true;
    
    log_debug("/dtn/bundle",
              "bundle id %d (%p): no more references, posting free event",
              bundleid_, this);

    BundleDaemon::instance()->post(new BundleFreeEvent(this));
    
    return 0;
}

//----------------------------------------------------------------------
Bundle::MappingsIterator
Bundle::mappings_begin()
{
    if (!lock_.is_locked_by_me())
        PANIC("Must lock Bundle before using mappings iterator");
    
    return mappings_.begin();
}
    
//----------------------------------------------------------------------
Bundle::MappingsIterator
Bundle::mappings_end()
{
    if (!lock_.is_locked_by_me())
        PANIC("Must lock Bundle before using mappings iterator");
    
    return mappings_.end();
}

//----------------------------------------------------------------------
bool
Bundle::is_queued_on(BundleList* bundle_list)
{
    oasys::ScopeLock l(&lock_, "Bundle::is_queued_on");
    return (mappings_.count(bundle_list) > 0);
}

//----------------------------------------------------------------------
bool
Bundle::validate(oasys::StringBuffer* errbuf)
{
    if (!source_.valid()) {
        errbuf->appendf("invalid source eid [%s]", source_.c_str());
        return false;
    }
    
    if (!dest_.valid()) {
        errbuf->appendf("invalid dest eid [%s]", dest_.c_str());
        return false;
    }

    if (!replyto_.valid()) {
        errbuf->appendf("invalid replyto eid [%s]", replyto_.c_str());
        return false;
    }

    if (!custodian_.valid()) {
        errbuf->appendf("invalid custodian eid [%s]", custodian_.c_str());
        return false;
    }

    return true;
    
}

} // namespace dtn

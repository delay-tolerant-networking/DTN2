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

#include <oasys/util/Time.h>
#include <oasys/debug/DebugUtils.h>
#include <oasys/thread/SpinLock.h>

#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleList.h"
#include "ExpirationTimer.h"

#include "storage/GlobalStore.h"
#include "storage/BundleStore.h"

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
    fragmented_incoming_= false;
    session_flags_      = 0;
    freed_          = false;
#ifdef BSP_ENABLED
    security_config_ = BundleSecurityConfig(Ciphersuite::config);
    payload_bek_ = NULL;
    payload_bek_len_ = 0;
    payload_bek_set_ = false;
    payload_encrypted_ = false;
    memset(payload_salt_, 0, 4);
    memset(payload_iv_, 0, 8);
    memset(payload_tag_, 0, 16);
#endif
    

    // as per the spec, the creation timestamp should be calculated as
    // seconds since 1/1/2000, and since the bundle id should be
    // monotonically increasing, it's safe to use that for the seqno

    // XXX/ modify this depending on whether a flag is set to use AEB or not.
    // XXX/ of course, we could just zero this out with our age block processor
    creation_ts_.seconds_ = BundleTimestamp::get_current_time();
    creation_ts_.seqno_   = bundleid_;

    age_                = 0; // [AEB]
    time_aeb_           = oasys::Time::now(); // [AEB]

    // This identifier provides information about when a local Bundle
    // object was created so that bundles with the same GBOF-ID can be
    // distinguished. We have to keep a copy separate from creation_ts_
    // because that will be set to the actual BP creation time if this
    // bundle was received from a peer, or is the result of
    // fragmentation, etc.

    // XXX/ hence, let's not break functionality by setting the internal
    // timestamp to 0. 
    extended_id_ = creation_ts_;

    log_debug_p("/dtn/bundle", "Bundle::init bundle id %d", id);
}

//----------------------------------------------------------------------
Bundle::Bundle(BundlePayload::location_t location)
    : Logger("Bundle", "/dtn/bundle/bundle"),
      payload_(&lock_), fwdlog_(&lock_, this), xmit_blocks_(&lock_),
      recv_metadata_("recv_metadata")
{
    u_int32_t id = GlobalStore::instance()->next_bundleid();
    init(id);
    payload_.init(id, location);
    refcount_	      = 0;
    expiration_timer_ = NULL;
    freed_	      = false;
}

//----------------------------------------------------------------------
Bundle::Bundle(const oasys::Builder&)
    : Logger("Bundle", "/dtn/bundle/bundle"),
      payload_(&lock_), fwdlog_(&lock_, this), xmit_blocks_(&lock_),
      recv_metadata_("recv_metadata")
{
    // don't do anything here except set the id to a bogus default
    // value and make sure the expiration timer is NULL, since the
    // fields are set and the payload initialized when loading from
    // the database
    init(0xffffffff);
    refcount_	      = 0;
    expiration_timer_ = NULL;
    freed_	      = false;
}

//----------------------------------------------------------------------
Bundle::~Bundle()
{
    log_debug_p("/dtn/bundle/free", "destroying bundle id %d", bundleid_);
    
    ASSERT(mappings_.size() == 0);
#ifdef BSP_ENABLED
    if(payload_bek_ != NULL) {
        free(payload_bek_);
    }
#endif
    bundleid_ = 0xdeadf00d;

    ASSERTF(expiration_timer_ == NULL,
            "bundle deleted with pending expiration timer");

}

//----------------------------------------------------------------------
int
Bundle::format(char* buf, size_t sz) const
{
    if (is_admin()) {
        return snprintf(buf, sz, "bundle id %u [%s -> %s %zu byte payload, is_admin]",
                        bundleid_, source_.c_str(), dest_.c_str(),
                        payload_.length());
    } else if (is_fragment()) {
        return snprintf(buf, sz, "bundle id %u [%s -> %s %zu byte payload, fragment @%u/%u]",
                        bundleid_, source_.c_str(), dest_.c_str(),
                        payload_.length(), frag_offset_, orig_length_);
    } else {
        return snprintf(buf, sz, "bundle id %u [%s -> %s %zu byte payload]",
                        bundleid_, source_.c_str(), dest_.c_str(),
                        payload_.length());
    }
}

//----------------------------------------------------------------------
void
Bundle::format_verbose(oasys::StringBuffer* buf)
{

#define bool_to_str(x)   ((x) ? "true" : "false")

    u_int32_t cur_time_sec = BundleTimestamp::get_current_time();

    buf->appendf("bundle id %d:\n", bundleid_);
    buf->appendf("            source: %s\n", source_.c_str());
    buf->appendf("              dest: %s\n", dest_.c_str());
    buf->appendf("         custodian: %s\n", custodian_.c_str());
    buf->appendf("           replyto: %s\n", replyto_.c_str());
    buf->appendf("           prevhop: %s\n", prevhop_.c_str());
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
    buf->appendf("       creation_ts: %llu.%llu\n",
                 creation_ts_.seconds_, creation_ts_.seqno_);
    buf->appendf("        expiration: %llu (%lld left)\n", expiration_,
                 creation_ts_.seconds_ + expiration_ - cur_time_sec);
    buf->appendf("       is_fragment: %s\n", bool_to_str(is_fragment_));
    buf->appendf("          is_admin: %s\n", bool_to_str(is_admin_));
    buf->appendf("   do_not_fragment: %s\n", bool_to_str(do_not_fragment_));
    buf->appendf("       orig_length: %d\n", orig_length_);
    buf->appendf("       frag_offset: %d\n", frag_offset_);
    buf->appendf("       sequence_id: %s\n", sequence_id_.to_str().c_str());
    buf->appendf("      obsoletes_id: %s\n", obsoletes_id_.to_str().c_str());
    buf->appendf("       session_eid: %s\n", session_eid_.c_str());
    buf->appendf("     session_flags: 0x%x\n", session_flags_);
    buf->appendf("               age: %llu\n", age_);
    //buf->appendf("          time_aeb: %llu\n", time_aeb_);
    buf->append("\n");

    buf->appendf("forwarding log:\n");
    fwdlog_.dump(buf);
    buf->append("\n");

    oasys::ScopeLock l(&lock_, "Bundle::format_verbose");
    buf->appendf("queued on %zu lists:\n", mappings_.size());
    for (BundleMappings::iterator i = mappings_.begin();
         i != mappings_.end(); ++i) {
        buf->appendf("\t%s\n", i->list()->name().c_str());
    }

    buf->append("\nrecv blocks:");
    for (BlockInfoVec::iterator iter = recv_blocks_.begin();
         iter != recv_blocks_.end();
         ++iter)
    {
        buf->appendf("\n type: 0x%02x ", iter->type());
        BlockProcessor *owner = BundleProtocol::find_processor(iter->type());
        if (iter->type()!=owner->block_type()){
        	iter->set_owner(owner);
        }
        if (iter->data_offset() == 0)
            buf->append("(runt)");
        else {
            if (!iter->complete())
                buf->append("(incomplete) ");
            buf->appendf("data length: %d", iter->full_length());
            buf->appendf(" -%d- ", owner->block_type());
            iter->owner()->format(buf, &(*iter));
        }
    }
    if (api_blocks_.size() > 0) {
        buf->append("\napi_blocks:");
        for (BlockInfoVec::iterator iter = api_blocks_.begin();
             iter != api_blocks_.end();
             ++iter)
        {
            buf->appendf("\n type: 0x%02x data length: %d",
                         iter->type(), iter->full_length());
            buf->append(" -- ");
            iter->owner()->format(buf, &(*iter));
       }
    } else {
    	buf->append("\nno api_blocks");
    }
    buf->append("\n");
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
    a->process("prevhop", &prevhop_);    
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
    a->process("session_eid", &session_eid_);    
    a->process("session_flags", &session_flags_);    
    a->process("extended_id_seconds", &extended_id_.seconds_);
    a->process("extended_id_seqno", &extended_id_.seqno_);
    a->process("recv_blocks", &recv_blocks_);
    a->process("api_blocks", &api_blocks_);

    a->process("age", &age_); // [AEB]
#ifdef BSP_ENABLED
    a->process("security_config", &security_config_);
    a->process("payload_bek_len", &payload_bek_len_);
    if(a->action_code() == oasys::Serialize::UNMARSHAL) {
        if(payload_bek_!= NULL) {
            free(payload_bek_);
        }
        payload_bek_ = (u_char *)malloc(payload_bek_len_);
    }
    a->process("payload_bek", payload_bek_, payload_bek_len_);
    a->process("payload_salt", payload_salt_, 4);
    a->process("payload_iv", payload_iv_, 8);
    a->process("payload_tag", payload_tag_, 16);
    a->process("payload_bek_set", &payload_bek_set_);
    a->process("payload_encrypted", &payload_encrypted_);
#endif
    //a->process("time_aeb", &time_aeb_); // [AEB]

    // a->process("metadata", &recv_metadata_); // XXX/kscott

    // serialize the forwarding log
    // Changed to the forwarding log result in the bundle being
    // updated on disk.
    log_debug("XXX Now serializing forwarding log");
    a->process("forwarding_log", &fwdlog_);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        in_datastore_ = true;
        payload_.init_from_store(bundleid_);
    }

    // Call consume() on each of the blocks?
}
    
//----------------------------------------------------------------------
void
Bundle::copy_metadata(Bundle* new_bundle) const
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
    new_bundle->age_	   	 	= age_; // [AEB]
    new_bundle->time_aeb_       = time_aeb_; // [AEB]
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
    log_debug_p("/dtn/bundle/refs",
                "bundle id %d (%p): refcount %d -> %d (%zu mappings) add %s %s",
                bundleid_, this, refcount_ - 1, refcount_,
                mappings_.size(), what1, what2);

    // if this is the first time we're adding a reference, then put it
    // on the all_bundles, which itself adds another reference to it.
    // note that we need to be careful to drop the scope lock before
    // calling push_back.
    if (ret == 1) {
        l.unlock(); // release scope lock
        BundleDaemon::instance()->all_bundles()->push_back(this);
    }
    
    return ret;
}

//----------------------------------------------------------------------
int
Bundle::del_ref(const char* what1, const char* what2)
{
    (void)what1;
    (void)what2;
    
    oasys::ScopeLock l(&lock_, "Bundle::del_ref");

    int ret = --refcount_;
    log_debug_p("/dtn/bundle/refs2",
                "bundle id %d (%p): freed_(%d) refcount %d -> %d (%zu mappings) del %s:%s",
                bundleid_, this, freed_, refcount_ + 1, refcount_,
                mappings_.size(), what1, what2);
#if 1
    log_debug_p("/dtn/bundle/refs2",
    		    "queued on %zu lists:\n", mappings_.size());
    for (BundleMappings::iterator i = mappings_.begin();
         i != mappings_.end(); ++i) {
        log_debug_p("/dtn/bundle/refs2", "\t%s\n", i->list()->name().c_str());
    }
#endif
    
    if (refcount_ > 1) {
        ASSERTF(freed_ == false,  "Bundle::del_ref on bundle %d (%p)"
                "called when bundle is freed but has %d references",
                bundleid_, this, refcount_);
    
        return ret;

    } else if (refcount_ == 1) {
        ASSERTF(freed_ == false,  "Bundle::del_ref on bundle %d (%p)"
                "called when bundle is freed but has %d references",
                bundleid_, this, refcount_);
        
        freed_ = true;
        
        log_debug_p("/dtn/bundle",
                    "bundle id %d (%p): one reference remaining, posting free event",
                    bundleid_, this);
        
        BundleDaemon::instance()->post(new BundleFreeEvent(this));

    } else if (refcount_ == 0) {
        log_debug_p("/dtn/bundle",
                    "bundle id %d (%p): last reference removed",
                    bundleid_, this);
        ASSERTF(freed_ == true,
                "Bundle %d (%p) refcount is zero but bundle wasn't properly freed",
                bundleid_, this);
   } else if (refcount_ < 0 ) {
	   log_debug_p("/dtn/bundle",
			       "bundle id %d (%p): refcount_(%d) < 0!",
			       bundleid_, this, refcount_);
   }
    
    return 0;
}

//----------------------------------------------------------------------
size_t
Bundle::num_mappings()
{
    oasys::ScopeLock l(&lock_, "Bundle::num_mappings");
    return mappings_.size();
}

//----------------------------------------------------------------------
BundleMappings*
Bundle::mappings()
{
    ASSERTF(lock_.is_locked_by_me(),
            "Must lock Bundle before using mappings iterator");
    
    return &mappings_;
}

//----------------------------------------------------------------------
bool
Bundle::is_queued_on(const BundleList* bundle_list)
{
    oasys::ScopeLock l(&lock_, "Bundle::is_queued_on");
    return mappings_.contains(bundle_list);
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

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

#include "Bundle.h"
#include "BundleEvent.h"
#include "BundleDaemon.h"
#include "BundleList.h"
#include "BundleRef.h"
#include "FragmentManager.h"

namespace dtn {

FragmentManager::FragmentManager()
    : Logger("/bundle/fragment")
{
    lock_ = new oasys::SpinLock();
}

Bundle* 
FragmentManager::create_fragment(Bundle* bundle, int offset, size_t length)
{
    Bundle* fragment = new Bundle();
    
    fragment->source_ = bundle->source_;
    fragment->replyto_ = bundle->replyto_;
    fragment->custodian_ = bundle->custodian_;
    fragment->dest_ = bundle->dest_;
    fragment->priority_ = bundle->priority_;
    fragment->custreq_ = bundle->custreq_;
    fragment->custody_rcpt_ = bundle->custody_rcpt_;
    fragment->recv_rcpt_ = bundle->recv_rcpt_;
    fragment->fwd_rcpt_ = bundle->fwd_rcpt_;
    fragment->return_rcpt_ = bundle->return_rcpt_;
    fragment->creation_ts_ = bundle->creation_ts_;
    fragment->expiration_ = bundle->expiration_;

    // always creating a fragment
    fragment->is_fragment_ = true;

    // initialize the fragment's orig_length and figure out the offset
    // into the payload
    if (! bundle->is_fragment_) {
        fragment->orig_length_ = bundle->payload_.length();
        fragment->frag_offset_ = offset;
    } else {
        fragment->orig_length_ = bundle->orig_length_;
        fragment->frag_offset_ = bundle->frag_offset_ + offset;
    }

    // check for overallocated length
    if ((offset + length) > fragment->orig_length_) {
        PANIC("fragment length overrun: "
              "orig_length %u frag_offset %u requested offset %u length %u",
              (u_int)fragment->orig_length_, (u_int)fragment->frag_offset_,
              (u_int)offset, (u_int)length);
    }


    // initialize payload
    fragment->payload_.set_length(length);
    fragment->payload_.write_data(&bundle->payload_, offset, length, 0);
    fragment->payload_.close_file();

    return fragment;
}

void
FragmentManager::convert_to_fragment(Bundle* bundle, size_t length)
{
    size_t payload_len = bundle->payload_.length();
    ASSERT(payload_len > length);

    if (! bundle->is_fragment_) {
        bundle->is_fragment_ = true;
        bundle->orig_length_ = payload_len;
        bundle->frag_offset_ = 0;
    } else {
        // if it was already a fragment, the fragment headers are
        // already correct
    }

    // truncate the payload
    bundle->payload_.truncate(length);
}

void
FragmentManager::get_hash_key(const Bundle* bundle, std::string* key)
{
    char buf[128];
    snprintf(buf, 128, "%lu.%lu",
             (unsigned long)bundle->creation_ts_.tv_sec,
             (unsigned long)bundle->creation_ts_.tv_usec);
    
    key->append(buf);
    key->append(bundle->source_.tuple());
    key->append(bundle->dest_.tuple());
}

/**
 * Reassembly state structure.
 */
struct FragmentManager::ReassemblyState {
    ReassemblyState()
        : fragments_("reassembly_state")
    {}
    
    Bundle* bundle_;		///< The reassembled bundle
    BundleList fragments_;	///< List of partial fragments
};

bool
FragmentManager::check_completed(ReassemblyState* state)
{
    Bundle* fragment;
    BundleList::iterator iter;

    size_t done_up_to = 0;  // running total of completed reassembly
    size_t f_len;
    size_t f_offset;
    size_t f_origlen;

    size_t total_len = state->bundle_->payload_.length();
    
    int fragi = 0;
    int fragn = state->fragments_.size();

    for (iter = state->fragments_.begin();
         iter != state->fragments_.end();
         ++iter, ++fragi)
    {
        fragment = *iter;

        f_len = fragment->payload_.length();
        f_offset = fragment->frag_offset_;
        f_origlen = fragment->orig_length_;
        
        ASSERT(fragment->is_fragment_);
        
        if (f_origlen != total_len) {
            PANIC("check_completed: error fragment orig len %u != total %u",
                  (u_int)f_origlen, (u_int)total_len);
            // XXX/demmer deal with this
        }

        if (done_up_to == f_offset) {
            /*
             * fragment is adjacent to the bytes so far
             * bbbbbbbbbb
             *           fff
             */
            log_debug("check_completed fragment %d/%d: "
                      "offset %u len %u total %u done_up_to %u: "
                      "(perfect fit)",
                      fragi, fragn, (u_int)f_offset, (u_int)f_len,
                      (u_int)f_origlen, (u_int)done_up_to);
            done_up_to += f_len;
        }

        else if (done_up_to < f_offset) {
            /*
             * there's a gap
             * bbbbbbb ffff
             */
            log_debug("check_completed fragment %d/%d: "
                      "offset %u len %u total %u done_up_to %u: "
                      "(found a hole)",
                      fragi, fragn, (u_int)f_offset, (u_int)f_len,
                      (u_int)f_origlen, (u_int)done_up_to);
            return false;

        }

        else if (done_up_to > (f_offset + f_len)) {
            /* fragment is completely redundant, skip
             * bbbbbbbbbb
             *      fffff
             */
            log_debug("check_completed fragment %d/%d: "
                      "offset %u len %u total %u done_up_to %u: "
                      "(redundant fragment)",
                      fragi, fragn, (u_int)f_offset, (u_int)f_len,
                      (u_int)f_origlen, (u_int)done_up_to);
            continue;
        }
        
        else if (done_up_to > f_offset) {
            /*
             * there's some overlap, so reduce f_len accordingly
             * bbbbbbbbbb
             *      fffffff
             */
            log_debug("check_completed fragment %d/%d: "
                      "offset %u len %u total %u done_up_to %u: "
                      "(overlapping fragment, reducing len to %u)",
                      fragi, fragn, (u_int)f_offset, (u_int)f_len,
                      (u_int)f_origlen, (u_int)done_up_to,
                      (u_int)(f_len - (done_up_to - f_offset)));
            
            f_len -= (done_up_to - f_offset);
            done_up_to += f_len;
        }

        else {
            // all cases should be covered above
            NOTREACHED;
        }
    }

    if (done_up_to == total_len) {
        log_debug("check_completed reassembly complete!");
        return true;
    } else {
        log_debug("check_completed reassembly not done (got %u/%u)",
                  (u_int)done_up_to, (u_int)total_len);
        return false;
    }
}

int
FragmentManager::proactively_fragment(Bundle* bundle, size_t max_length)
{
    size_t payload_len = bundle->payload_.length();
    
    if (max_length == 0 || max_length > payload_len) {
        return 0;
    }

    log_info("proactively fragmenting "
         "%u byte bundle into %u %u byte fragments",
         (u_int)payload_len, (u_int)(payload_len / max_length),
         (u_int)max_length);

    Bundle* fragment;
    size_t todo = payload_len;
    size_t offset = 0;
    size_t fraglen = max_length;
    size_t count = 0;
    
    do {
        if ((offset + fraglen) > payload_len) {
            fraglen = payload_len - offset; // tail
        }
        ASSERT(todo >= fraglen);
        
        fragment = create_fragment(bundle, offset, fraglen);
        ASSERT(fragment);
        
        BundleDaemon::post(
            new BundleReceivedEvent(fragment,
                                    EVENTSRC_FRAGMENTATION,
                                    fraglen));
        offset += fraglen;
        todo -= fraglen;
        ++count;
        
    } while (todo > 0);

    bundle->payload_.close_file();
    return count;
}


/**
 * If only part of the given bundle was sent successfully, create
 * a new fragment for the unsent portion.
 *
 * Return 1 if a fragment was created, 0 otherwise.
 */
int
FragmentManager::reactively_fragment(Bundle* bundle, size_t bytes_sent)
{
    size_t payload_len = bundle->payload_.length();
    
    if (bytes_sent == payload_len)
    {
        return 0; // nothing to do
    }
    
    size_t frag_off = bytes_sent;
    size_t frag_len = payload_len - bytes_sent;

    log_debug("creating reactive fragment (offset %u len %u/%u)",
              (u_int)frag_off, (u_int)frag_len, (u_int)payload_len);
    
    Bundle* tail = create_fragment(bundle, frag_off, frag_len);
    bundle->payload_.close_file();

    // XXX/demmer temp to put it on the head of the contact list
    tail->is_reactive_fragment_ = true;

    // treat the new fragment as if it just arrived
    BundleDaemon::post(
        new BundleReceivedEvent(tail, EVENTSRC_FRAGMENTATION, frag_len));

    return 1;
}


Bundle* 
FragmentManager::process_for_reassembly(Bundle* fragment)
{
    oasys::ScopeLock l(lock_);

    ReassemblyState* state;
    ReassemblyTable::iterator iter;

    ASSERT(fragment->is_fragment_);

    // cons up the key to do the table lookup and look for reassembly state
    std::string hash_key;
    get_hash_key(fragment, &hash_key);
    iter = reassembly_table_.find(hash_key);

    log_debug("processing bundle fragment id=%u hash=%s %d",
              fragment->bundleid_, hash_key.c_str(), fragment->is_fragment_);

    if (iter == reassembly_table_.end()) {
        log_debug("no reassembly state for key %s -- creating new state",
                  hash_key.c_str());
        state = new ReassemblyState();

        state->bundle_ = new Bundle();
        state->bundle_->payload_.set_length(fragment->orig_length_,
                                            BundlePayload::DISK);
        state->bundle_->add_ref("reassembly_state");
        
        reassembly_table_[hash_key] = state;
    } else {
        state = iter->second;
        log_debug("found reassembly state for key %s (%u fragments)",
                  hash_key.c_str(), (u_int)state->fragments_.size());

        state->bundle_->payload_.reopen_file();
    }

    // grab a lock on the fragment list and tack on the new fragment
    // to the fragment list
    oasys::ScopeLock fraglock(state->fragments_.lock());
    state->fragments_.insert_sorted(fragment, BundleList::SORT_FRAG_OFFSET);
    
    // store the fragment data in the partially reassembled bundle file
    size_t fraglen = fragment->payload_.length();

    log_debug(
              "write_data: length_=%u src_offset=%u dst_offset=%u len %u",
              (u_int)state->bundle_->payload_.length(), 
              0, fragment->frag_offset_, (u_int)fraglen);

    state->bundle_->payload_.write_data(&fragment->payload_, 0, fraglen,
                                        fragment->frag_offset_);
    state->bundle_->payload_.close_file();
    
    // check see if we're done
    if (!check_completed(state)) {
        return NULL;
    }

    Bundle* ret = state->bundle_;
    
    BundleDaemon::post
        (new ReassemblyCompletedEvent(ret, &state->fragments_));
    
    state->bundle_->del_ref("reassembly_state");
    state->fragments_.clear();
    reassembly_table_.erase(hash_key);
    fraglock.unlock();
    delete state;
    
    return ret;
}



} // namespace dtn

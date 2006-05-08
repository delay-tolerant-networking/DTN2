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

//----------------------------------------------------------------------
FragmentManager::FragmentManager()
    : Logger("FragmentManager", "/dtn/bundle/fragmentation")
{
}

//----------------------------------------------------------------------
Bundle* 
FragmentManager::create_fragment(Bundle* bundle, size_t offset, size_t length)
{
    Bundle* fragment = new Bundle();

    // copy the metadata into the new  fragment (which can be further fragmented)
    bundle->copy_metadata(fragment);
    fragment->is_fragment_     = true;
    fragment->do_not_fragment_ = false;
    
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
              "orig_length %u frag_offset %u requested offset %zu length %zu",
              fragment->orig_length_, fragment->frag_offset_,
              offset, length);
    }


    // initialize payload
    fragment->payload_.set_length(length);
    fragment->payload_.write_data(&bundle->payload_, offset, length, 0);
    fragment->payload_.close_file();

    return fragment;
}

//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
void
FragmentManager::get_hash_key(const Bundle* bundle, std::string* key)
{
    char buf[128];
    snprintf(buf, 128, "%lu.%lu",
             (unsigned long)bundle->creation_ts_.tv_sec,
             (unsigned long)bundle->creation_ts_.tv_usec);
    
    key->append(buf);
    key->append(bundle->source_.c_str());
    key->append(bundle->dest_.c_str());
}

//----------------------------------------------------------------------
bool
FragmentManager::check_completed(ReassemblyState* state)
{
    Bundle* fragment;
    BundleList::iterator iter;
    oasys::ScopeLock l(state->fragments_.lock(),
                       "FragmentManager::check_completed");
    
    size_t done_up_to = 0;  // running total of completed reassembly
    size_t f_len;
    size_t f_offset;
    size_t f_origlen;

    size_t total_len = state->bundle_->payload_.length();
    
    int fragi = 0;
    int fragn = state->fragments_.size();
    (void)fragn; // in case NDEBUG is defined

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
            PANIC("check_completed: error fragment orig len %zu != total %zu",
                  f_origlen, total_len);
            // XXX/demmer deal with this
        }

        if (done_up_to == f_offset) {
            /*
             * fragment is adjacent to the bytes so far
             * bbbbbbbbbb
             *           fff
             */
            log_debug("check_completed fragment %d/%d: "
                      "offset %zu len %zu total %zu done_up_to %zu: "
                      "(perfect fit)",
                      fragi, fragn, f_offset, f_len, f_origlen, done_up_to);
            done_up_to += f_len;
        }

        else if (done_up_to < f_offset) {
            /*
             * there's a gap
             * bbbbbbb ffff
             */
            log_debug("check_completed fragment %d/%d: "
                      "offset %zu len %zu total %zu done_up_to %zu: "
                      "(found a hole)",
                      fragi, fragn, f_offset, f_len, f_origlen, done_up_to);
            return false;

        }

        else if (done_up_to > (f_offset + f_len)) {
            /* fragment is completely redundant, skip
             * bbbbbbbbbb
             *      fffff
             */
            log_debug("check_completed fragment %d/%d: "
                      "offset %zu len %zu total %zu done_up_to %zu: "
                      "(redundant fragment)",
                      fragi, fragn, f_offset, f_len, f_origlen, done_up_to);
            continue;
        }
        
        else if (done_up_to > f_offset) {
            /*
             * there's some overlap, so reduce f_len accordingly
             * bbbbbbbbbb
             *      fffffff
             */
            log_debug("check_completed fragment %d/%d: "
                      "offset %zu len %zu total %zu done_up_to %zu: "
                      "(overlapping fragment, reducing len to %zu)",
                      fragi, fragn, f_offset, f_len, f_origlen, done_up_to,
                      (f_len - (done_up_to - f_offset)));
            
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
        log_debug("check_completed reassembly not done (got %zu/%zu)",
                  done_up_to, total_len);
        return false;
    }
}

//----------------------------------------------------------------------
int
FragmentManager::proactively_fragment(Bundle* bundle, size_t max_length)
{
    size_t payload_len = bundle->payload_.length();
    
    if (max_length == 0 || max_length > payload_len) {
        return 0;
    }

    log_info("proactively fragmenting "
         "%zu byte bundle into %zu %zu byte fragments",
         payload_len, (payload_len / max_length), max_length);

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

//----------------------------------------------------------------------
bool
FragmentManager::try_to_reactively_fragment(Bundle* bundle, size_t bytes_sent)
{
    size_t payload_len = bundle->payload_.length();
    
    if ((bytes_sent == 0) || (bytes_sent == payload_len))
    {
        return false; // nothing to do
    }
    
    size_t frag_off = bytes_sent;
    size_t frag_len = payload_len - bytes_sent;

    log_debug("creating reactive fragment (offset %zu len %zu/%zu)",
              frag_off, frag_len, payload_len);
    
    Bundle* tail = create_fragment(bundle, frag_off, frag_len);
    bundle->payload_.close_file();

    // treat the new fragment as if it just arrived
    BundleDaemon::post_at_head(
        new BundleReceivedEvent(tail, EVENTSRC_FRAGMENTATION, frag_len));

    return true;
}

//----------------------------------------------------------------------
void
FragmentManager::process_for_reassembly(Bundle* fragment)
{
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

        // copy the metadata from the first fragment to arrive, but
        // make sure we mark the bundle that it's not a fragment (or
        // at least won't be for long)
        state->bundle_ = new Bundle();
        fragment->copy_metadata(state->bundle_.object());
        state->bundle_->is_fragment_ = false;
        
        state->bundle_->payload_.set_length(fragment->orig_length_,
                                            BundlePayload::DISK);
        reassembly_table_[hash_key] = state;
    } else {
        state = iter->second;
        log_debug("found reassembly state for key %s (%zu fragments)",
                  hash_key.c_str(), state->fragments_.size());

        state->bundle_->payload_.reopen_file();
    }

    // stick the fragment on the reassembly list
    state->fragments_.insert_sorted(fragment, BundleList::SORT_FRAG_OFFSET);
    
    // store the fragment data in the partially reassembled bundle file
    size_t fraglen = fragment->payload_.length();
    
    log_debug("write_data: length_=%zu src_offset=%u dst_offset=%u len %zu",
              state->bundle_->payload_.length(), 
              0, fragment->frag_offset_, fraglen);

    state->bundle_->payload_.write_data(&fragment->payload_, 0, fraglen,
                                        fragment->frag_offset_);
    state->bundle_->payload_.close_file();
    
    // check see if we're done
    if (!check_completed(state)) {
        return;
    }

    BundleDaemon::post_at_head
        (new ReassemblyCompletedEvent(state->bundle_.object(),
                                      &state->fragments_));
    ASSERT(state->fragments_.size() == 0); // moved into the event
    reassembly_table_.erase(hash_key);
    delete state;
}



} // namespace dtn

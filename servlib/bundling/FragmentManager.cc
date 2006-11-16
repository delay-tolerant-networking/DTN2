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
bool
FragmentManager::try_to_convert_to_fragment(Bundle* bundle,
                                            size_t  payload_offset,
                                            size_t  bytes_rcvd)
{
    if (bytes_rcvd <= payload_offset) {
        return false; // can't do anything
    }
    
    size_t payload_len  = bundle->payload_.length();
    size_t payload_rcvd = std::min(payload_len, bytes_rcvd - payload_offset);

    if (payload_rcvd >= payload_len) {
        return false; // nothing to do
    }
    
    log_debug("partial bundle *%p, making reactive fragment of %zu bytes",
              bundle, bytes_rcvd);
        
    if (! bundle->is_fragment_) {
        bundle->is_fragment_ = true;
        bundle->orig_length_ = payload_len;
        bundle->frag_offset_ = 0;
    } else {
        // if it was already a fragment, the fragment headers are
        // already correct
    }
    
    bundle->payload_.truncate(payload_rcvd);
    
    return true;
}

//----------------------------------------------------------------------
void
FragmentManager::get_hash_key(const Bundle* bundle, std::string* key)
{
    char buf[128];
    snprintf(buf, 128, "%u.%u",
             bundle->creation_ts_.seconds_,
             bundle->creation_ts_.seqno_);
    
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
            new BundleReceivedEvent(fragment, EVENTSRC_FRAGMENTATION));
        offset += fraglen;
        todo -= fraglen;
        ++count;
        
    } while (todo > 0);

    bundle->payload_.close_file();
    return count;
}

//----------------------------------------------------------------------
bool
FragmentManager::try_to_reactively_fragment(Bundle* bundle,
                                            size_t  payload_offset,
                                            size_t  bytes_sent)
{
    if (bytes_sent <= payload_offset) {
        return false; // can't do anything
    }
    
    size_t payload_len  = bundle->payload_.length();
    size_t payload_sent = std::min(payload_len, bytes_sent - payload_offset);
    
    if (payload_sent >= payload_len) {
        return false; // nothing to do
    }
    
    size_t frag_off = payload_sent;
    size_t frag_len = payload_len - payload_sent;

    log_debug("creating reactive fragment (offset %zu len %zu/%zu)",
              frag_off, frag_len, payload_len);
    
    Bundle* tail = create_fragment(bundle, frag_off, frag_len);
    bundle->payload_.close_file();

    // treat the new fragment as if it just arrived
    BundleDaemon::post_at_head(
        new BundleReceivedEvent(tail, EVENTSRC_FRAGMENTATION));

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

//----------------------------------------------------------------------
void
FragmentManager::delete_fragment(Bundle* fragment)
{
    ReassemblyState* state;
    ReassemblyTable::iterator iter;

    ASSERT(fragment->is_fragment_);

    // cons up the key to do the table lookup and look for reassembly state
    std::string hash_key;
    get_hash_key(fragment, &hash_key);
    iter = reassembly_table_.find(hash_key);

    // no reassembly state, simply return
    if (iter == reassembly_table_.end()) {
        return;
    }

    state = iter->second;

    // remove the fragment from the reassembly list
    bool erased = state->fragments_.erase(fragment);

    // fragment was not in reassembly list, simply return
    if (!erased) {
        return;
    }

    // create a null buffer; the "null" character is used as padding in
    // the partially reassembled bundle file
    u_char buf[fragment->payload_.length()];
    memset(buf, '\0', fragment->payload_.length());
    
    // remove the fragment data from the partially reassembled bundle file
    state->bundle_->payload_.reopen_file();
    state->bundle_->payload_.write_data(buf, fragment->frag_offset_,
                                        fragment->payload_.length());
    state->bundle_->payload_.close_file();

    // delete reassembly state if no fragments now exist
    if (state->fragments_.size() == 0) {
        reassembly_table_.erase(hash_key);
        delete state;
    }
}

} // namespace dtn


#include "Bundle.h"
#include "BundleEvent.h"
#include "BundleForwarder.h"
#include "BundleList.h"
#include "BundleRef.h"
#include "FragmentManager.h"

FragmentManager FragmentManager::instance_;

FragmentManager::FragmentManager()
    : Logger("/bundle/fragment")
{
    lock_ = new SpinLock();
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
              "orig_length %d frag_offset %d requested offset %d length %d",
              fragment->orig_length_, fragment->frag_offset_,
              offset, length);
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
             bundle->creation_ts_.tv_sec,
             bundle->creation_ts_.tv_usec);
    
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
            PANIC("check_completed: error fragment orig len %d != total %d",
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
                      "offset %d len %d total %d done_up_to %d: "
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
                      "offset %d len %d total %d done_up_to %d: "
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
                      "offset %d len %d total %d done_up_to %d: "
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
                      "offset %d len %d total %d done_up_to %d: "
                      "(overlapping fragment, reducing len to %d)",
                      fragi, fragn, f_offset, f_len, f_origlen, done_up_to,
                      f_len - (done_up_to - f_offset));
            
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
        log_debug("check_completed reassembly not done (got %d/%d)",
                  done_up_to, total_len);
        return false;
    }
}

Bundle* 
FragmentManager::process(Bundle* fragment)
{
    ScopeLock l(lock_);

    ReassemblyState* state;
    ReassemblyTable::iterator iter;

    ASSERT(fragment->is_fragment_);

    // cons up the key to do the table lookup and look for reassembly state
    std::string hash_key;
    get_hash_key(fragment, &hash_key);
    iter = reassembly_table_.find(hash_key);
    
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
        log_debug("found reassembly state for key %s (%d fragments)",
                  hash_key.c_str(), state->fragments_.size());

        state->bundle_->payload_.reopen_file();
    }

    // grab a lock on the fragment list and tack on the new fragment
    // to the fragment list
    ScopeLock fraglock(state->fragments_.lock());
    state->fragments_.insert_sorted(fragment, NULL,
                                    BundleList::SORT_FRAG_OFFSET);
    
    // store the fragment data in the partially reassembled bundle file
    size_t fraglen = fragment->payload_.length();
    state->bundle_->payload_.write_data(&fragment->payload_, 0, fraglen,
                                        fragment->frag_offset_);
    state->bundle_->payload_.close_file();
    
    // check see if we're done
    if (!check_completed(state)) {
        return NULL;
    }

    Bundle* ret = state->bundle_;
    
    BundleForwarder::post
        (new ReassemblyCompletedEvent(ret, &state->fragments_));
    
    state->bundle_->del_ref("reassembly_state");
    state->fragments_.clear();
    reassembly_table_.erase(hash_key);
    fraglock.unlock();
    delete state;
    
    return ret;
}



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
#  include <config.h>
#endif

#include <list>

#include "Bundle.h"
#include "BundleEvent.h"
#include "BundleDaemon.h"
#include "BundleList.h"
#include "BundleRef.h"
#include "FragmentManager.h"
#include "FragmentState.h"
#include "BlockInfo.h"
#include "BundleProtocol.h"

namespace dtn {
    
class BlockInfoPointerList : public std::list<BlockInfo*> { };

//----------------------------------------------------------------------
FragmentManager::FragmentManager()
    : Logger("FragmentManager", "/dtn/bundle/fragmentation")
{
}

//----------------------------------------------------------------------
Bundle* 
FragmentManager::create_fragment(Bundle* bundle,
                                 BlockInfoVec *blocks,
                                 size_t offset,
                                 size_t length)
{
    Bundle* fragment = new Bundle();

    // copy the metadata into the new fragment (which can be further fragmented)
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

    // copy all blocks that follow the payload, and all those before
    // the payload that are marked with the "must be replicated in every
    // fragment" bit
    BlockInfoVec::iterator iter;
    bool found_payload = false;
    for (iter = blocks->begin(); iter != blocks->end(); iter++) {
        int type = iter->type();
        if (type == BundleProtocol::PRIMARY_BLOCK
            || type == BundleProtocol::PAYLOAD_BLOCK
            || found_payload
            || iter->flags() & BundleProtocol::BLOCK_FLAG_REPLICATE) {

            // we need to include this block; copy the BlockInfo into the
            // fragment
            fragment->recv_blocks_.push_back(*iter);
            if (type == BundleProtocol::PAYLOAD_BLOCK) {
                found_payload = true;
            }
        }
    }

    return fragment;
}

//----------------------------------------------------------------------
Bundle* 
FragmentManager::create_fragment(Bundle* bundle,
                                 const LinkRef& link,
                                 const BlockInfoPointerList& blocks_to_copy,
                                 size_t offset,
                                 size_t max_length)
{
    size_t block_length = 0;
    BlockInfoPointerList::const_iterator block_i;
    
    for (block_i = blocks_to_copy.begin();
         block_i != blocks_to_copy.end();
         ++block_i) {
        block_length += (*block_i)->contents().len();
    }
        
    if (block_length > max_length) {
        log_err("unable to create a fragment of length %zu; minimum length "
                "required is %zu", max_length, block_length);
        return NULL;
    }
    
    Bundle* fragment = new Bundle();

    // copy the metadata into the new fragment (which can be further fragmented)
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

    // initialize payload
    size_t to_copy = std::min(max_length - block_length,
                              bundle->payload_.length() - offset);
    fragment->payload_.set_length(to_copy);
    fragment->payload_.write_data(&bundle->payload_, offset, to_copy, 0);
    BlockInfoVec* xmit_blocks = fragment->xmit_blocks_.create_blocks(link);
    
    for (block_i = blocks_to_copy.begin();
         block_i != blocks_to_copy.end();
         ++block_i) {
        xmit_blocks->push_back(BlockInfo(*(*block_i)));
    }
    
    log_debug("created %zu byte fragment bundle with %zu bytes of payload",
              to_copy + block_length, to_copy);

    return fragment;
}

//----------------------------------------------------------------------
bool
FragmentManager::try_to_convert_to_fragment(Bundle* bundle)
{
    const BlockInfo *payload_block
        = bundle->recv_blocks_.find_block(BundleProtocol::PAYLOAD_BLOCK);
    if (!payload_block) {
        return false; // can't do anything
    }
    if (payload_block->data_offset() == 0) {
        return false; // there is not even enough data for the preamble
    }

    if (bundle->do_not_fragment_) {
        return false; // can't do anything
    }

    // the payload is already truncated to the length that was received
    size_t payload_len  = payload_block->data_length();
    size_t payload_rcvd = bundle->payload_.length();

    // A fragment cannot be created with only one byte of payload
    // available.
    if (payload_len <= 1) {
        return false;
    }

    if (payload_rcvd >= payload_len) {
        ASSERT(payload_block->complete() || payload_len == 0);

        if (payload_block->last_block()) {
            return false; // nothing to do - whole bundle present
        }

        // If the payload block is not the last block, there are extension
        // blocks following it. See if they all appear to be present.
        BlockInfoVec::iterator last_block = bundle->recv_blocks_.end() - 1;
        if (last_block->data_offset() != 0 && last_block->complete()
            && last_block->last_block()) {
            return false; // nothing to do - whole bundle present
        }

        // At this point the payload is complete but the bundle is not,
        // so force the creation of a fragment by dropping a byte.
        payload_rcvd--;
        bundle->payload_.truncate(payload_rcvd);
    }
    
    log_debug("partial bundle *%p, making reactive fragment of %zu bytes",
              bundle, payload_rcvd);
        
    if (! bundle->is_fragment_) {
        bundle->is_fragment_ = true;
        bundle->orig_length_ = payload_len;
        bundle->frag_offset_ = 0;
    } else {
        // if it was already a fragment, the fragment headers are
        // already correct
    }
    bundle->fragmented_incoming_ = true;
    
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
FragmentState*
FragmentManager::proactively_fragment(Bundle* bundle, 
                                      const LinkRef& link,
                                      size_t max_length)
{
    size_t payload_len = bundle->payload_.length();
    
    Bundle* fragment;
    FragmentState* state = new FragmentState(bundle);
    
    size_t todo = payload_len;
    size_t offset = 0;
    size_t count = 0;
    
    BlockInfoPointerList first_frag_blocks;
    BlockInfoPointerList all_frag_blocks;
    BlockInfoPointerList& this_frag_blocks = first_frag_blocks;
    BlockInfoVec* blocks = bundle->xmit_blocks_.find_blocks(link);
    
    BlockInfoVec::iterator block_i;
    for (block_i = blocks->begin(); block_i != blocks->end(); ++block_i) {
        BlockInfo* block_info = &(*block_i);
        
        if (block_info->type() == BundleProtocol::PRIMARY_BLOCK ||
        block_info->type() == BundleProtocol::PAYLOAD_BLOCK) {
            all_frag_blocks.push_back(block_info);
            first_frag_blocks.push_back(block_info);
        }
        
        else if (block_info->flags() & BundleProtocol::BLOCK_FLAG_REPLICATE)
            all_frag_blocks.push_back(block_info);
        else
            first_frag_blocks.push_back(block_info);
    }
    
    do {
        fragment = create_fragment(bundle, link, this_frag_blocks, 
                                   offset, max_length);
        ASSERT(fragment);
        
        state->add_fragment(fragment);
        offset += fragment->payload_.length();
        todo -= fragment->payload_.length();
        this_frag_blocks = all_frag_blocks;
        ++count;
        
    } while (todo > 0);
    
    log_info("proactively fragmenting "
            "%zu byte payload into %zu %zu byte fragments",
            payload_len, count, max_length);
    
    std::string hash_key;
    get_hash_key(fragment, &hash_key);
    fragment_table_[hash_key] = state;

    return state;
}

FragmentState*
FragmentManager::get_fragment_state(Bundle* bundle)
{
    std::string hash_key;
    get_hash_key(bundle, &hash_key);
    FragmentTable::iterator iter = fragment_table_.find(hash_key);

    if (iter == fragment_table_.end()) {
        return NULL;
    } else {
        return iter->second;
    }
}

//----------------------------------------------------------------------
void
FragmentManager::erase_fragment_state(FragmentState* state)
{
    std::string hash_key;
    get_hash_key(state->bundle().object(), &hash_key);
    fragment_table_.erase(hash_key);
}

//----------------------------------------------------------------------
bool
FragmentManager::try_to_reactively_fragment(Bundle* bundle,
                                            BlockInfoVec *blocks,
                                            size_t  bytes_sent)
{
    if (bundle->do_not_fragment_) {
        return false; // can't do anything
    }

    size_t payload_offset = BundleProtocol::payload_offset(blocks);
    size_t total_length = BundleProtocol::total_length(blocks);

    if (bytes_sent <= payload_offset) {
        return false; // can't do anything
    }

    if (bytes_sent >= total_length) {
        return false; // nothing to do
    }
    
    const BlockInfo *payload_block
        = blocks->find_block(BundleProtocol::PAYLOAD_BLOCK);

    size_t payload_len  = bundle->payload_.length();
    size_t payload_sent = std::min(payload_len, bytes_sent - payload_offset);

    // A fragment cannot be created with only one byte of payload
    // available.
    if (payload_len <= 1) {
        return false;
    }

    size_t frag_off, frag_len;

    if (payload_sent >= payload_len) {
        // this means some but not all data after the payload was transmitted
        ASSERT(! payload_block->last_block());

        // keep a byte to put with the trailing blocks
        frag_off = payload_len - 1;
        frag_len = 1;
    }
    else {
        frag_off = payload_sent;
        frag_len = payload_len - payload_sent;
    }

    log_debug("creating reactive fragment (offset %zu len %zu/%zu)",
              frag_off, frag_len, payload_len);
    
    Bundle* tail = create_fragment(bundle, blocks, frag_off, frag_len);

    // treat the new fragment as if it just arrived
    BundleDaemon::post_at_head(
        new BundleReceivedEvent(tail, EVENTSRC_FRAGMENTATION));

    return true;
}

//----------------------------------------------------------------------
void
FragmentManager::process_for_reassembly(Bundle* fragment)
{
    FragmentState* state;
    FragmentTable::iterator iter;

    ASSERT(fragment->is_fragment_);

    // cons up the key to do the table lookup and look for reassembly state
    std::string hash_key;
    get_hash_key(fragment, &hash_key);
    iter = fragment_table_.find(hash_key);

    log_debug("processing bundle fragment id=%u hash=%s %d",
              fragment->bundleid_, hash_key.c_str(), fragment->is_fragment_);

    if (iter == fragment_table_.end()) {
        log_debug("no reassembly state for key %s -- creating new state",
                  hash_key.c_str());
        state = new FragmentState();

        // copy the metadata from the first fragment to arrive, but
        // make sure we mark the bundle that it's not a fragment (or
        // at least won't be for long)
        fragment->copy_metadata(state->bundle().object());
        state->bundle()->is_fragment_ = false;
        state->bundle()->payload_.set_length(fragment->orig_length_);
        fragment_table_[hash_key] = state;
    } else {
        state = iter->second;
        log_debug("found reassembly state for key %s (%zu fragments)",
                  hash_key.c_str(), state->fragment_list().size());
    }

    // stick the fragment on the reassembly list
    state->add_fragment(fragment);
    
    // store the fragment data in the partially reassembled bundle file
    size_t fraglen = fragment->payload_.length();
    
    log_debug("write_data: length_=%zu src_offset=%u dst_offset=%u len %zu",
              state->bundle()->payload_.length(), 
              0, fragment->frag_offset_, fraglen);

    state->bundle()->payload_.write_data(&fragment->payload_, 0, fraglen,
                                         fragment->frag_offset_);
    
    // XXX/jmmikkel this ensures that we have a set of blocks in the
    // reassembled bundle, but eventually reassembly will have to do much more
    if (fragment->frag_offset_ == 0 && !state->bundle()->recv_blocks_.empty()) {
        for (BlockInfoVec::iterator block_i = fragment->recv_blocks_.begin();
             block_i != fragment->recv_blocks_.end();
             ++block_i) {
            state->bundle()->recv_blocks_.push_back(BlockInfo(*block_i));
        }
    }
    
    // check see if we're done
    if (! state->check_completed()) {
        return;
    }

    BundleDaemon::post_at_head
        (new ReassemblyCompletedEvent(state->bundle().object(),
                                      &state->fragment_list()));
    ASSERT(state->fragment_list().size() == 0); // moved into the event
    fragment_table_.erase(hash_key);
    delete state;
}

//----------------------------------------------------------------------
void
FragmentManager::delete_fragment(Bundle* fragment)
{
    FragmentState* state;
    FragmentTable::iterator iter;

    ASSERT(fragment->is_fragment_);

    // cons up the key to do the table lookup and look for reassembly state
    std::string hash_key;
    get_hash_key(fragment, &hash_key);
    iter = fragment_table_.find(hash_key);

    // no reassembly state, simply return
    if (iter == fragment_table_.end()) {
        return;
    }

    state = iter->second;

    // remove the fragment from the reassembly list
    bool erased = state->erase_fragment(fragment);

    // fragment was not in reassembly list, simply return
    if (!erased) {
        return;
    }

    // create a null buffer; the "null" character is used as padding in
    // the partially reassembled bundle file
    u_char buf[fragment->payload_.length()];
    memset(buf, '\0', fragment->payload_.length());
    
    // remove the fragment data from the partially reassembled bundle file
    state->bundle()->payload_.write_data(buf, fragment->frag_offset_,
                                         fragment->payload_.length());

    // delete reassembly state if no fragments now exist
    if (state->num_fragments() == 0) {
        fragment_table_.erase(hash_key);
        delete state;
    }
}

} // namespace dtn

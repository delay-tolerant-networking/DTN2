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

#include <sys/types.h>
#include <netinet/in.h>

#include <oasys/debug/DebugUtils.h>
#include <oasys/util/StringUtils.h>

#include "BundleProtocol.h"
#include "BlockInfo.h"
#include "BlockProcessor.h"
#include "Bundle.h"
#include "BundleTimestamp.h"
#include "PayloadBlockProcessor.h"
#include "PreviousHopBlockProcessor.h"
#include "PrimaryBlockProcessor.h"
#include "SDNV.h"
#include "UnknownBlockProcessor.h"
#include "MetadataBlockProcessor.h"

#ifdef BSP_ENABLED
#  include "security/SPD.h"
#endif

namespace dtn {

static const char* LOG = "/dtn/bundle/protocol";

BlockProcessor* BundleProtocol::processors_[256];

//----------------------------------------------------------------------
void
BundleProtocol::register_processor(BlockProcessor* bp)
{
    // can't override an existing processor
    ASSERT(processors_[bp->block_type()] == 0);
    processors_[bp->block_type()] = bp;
}

//----------------------------------------------------------------------
BlockProcessor*
BundleProtocol::find_processor(u_int8_t type)
{
    BlockProcessor* ret = processors_[type];
    if (ret == 0) {
        ret = UnknownBlockProcessor::instance();
    }
    return ret;
}

//----------------------------------------------------------------------
void
BundleProtocol::init_default_processors()
{
    // register default block processor handlers
    BundleProtocol::register_processor(new PrimaryBlockProcessor());
    BundleProtocol::register_processor(new PreviousHopBlockProcessor());
    BundleProtocol::register_processor(new PayloadBlockProcessor());
    BundleProtocol::register_processor(new MetadataBlockProcessor());
}

//----------------------------------------------------------------------
BlockInfoVec*
BundleProtocol::prepare_blocks(Bundle* bundle, const LinkRef& link)
{
    // create a new block list for the outgoing link by first calling
    // prepare on all the BlockProcessor classes for the blocks that
    // arrived on the link
    BlockInfoVec* xmit_blocks = bundle->xmit_blocks_.create_blocks(link);
    BlockInfoVec* recv_blocks = &bundle->recv_blocks_;
    BlockInfoVec* api_blocks = &bundle->api_blocks_;

    if (recv_blocks->size() > 0) {
        // if there is a received block, the first one better be the primary
        ASSERT(recv_blocks->front().type() == PRIMARY_BLOCK);
    
        for (BlockInfoVec::iterator iter = recv_blocks->begin();
             iter != recv_blocks->end();
             ++iter)
        {
            // if this block follows the payload block, and the bundle was
            // reactively fragmented, this block should be in the later
            // fragment, so don't include it
            //
            // XXX/demmer it seems to me like this should just break
            // out of the loop once it's processed the PAYLOAD_BLOCK
            // if fragmented_incoming_ is true
            if (bundle->fragmented_incoming_
                && xmit_blocks->find_block(BundleProtocol::PAYLOAD_BLOCK)) {
                continue;
            }
            
            // allow BlockProcessors [and Ciphersuites] a chance to re-do things 
            // needed after a possible load-from-store
            iter->owner()->reload_post_process(bundle, recv_blocks, &*iter);     
            
            iter->owner()->prepare(bundle, xmit_blocks, &*iter,link,  BlockInfo::LIST_RECEIVED);
        }
    }
    else {
        log_debug_p(LOG, "adding primary and payload block");
        BlockProcessor* bp = find_processor(PRIMARY_BLOCK);
        bp->prepare(bundle, xmit_blocks, NULL, link, BlockInfo::LIST_NONE);
        bp = find_processor(PAYLOAD_BLOCK);
        bp->prepare(bundle, xmit_blocks, NULL, link, BlockInfo::LIST_NONE);
    }

    // locally generated bundles need to include blocks specified at the API
    for (BlockInfoVec::iterator iter = api_blocks->begin();
         iter != api_blocks->end();
         ++iter)
    {
        log_debug_p(LOG, "adding api_block");
        iter->owner()->prepare(bundle, xmit_blocks, 
                               &*iter, link, BlockInfo::LIST_API);
    }

    // now we also make sure to prepare() on any registered processors
    // that don't already have a block in the output list. this
    // handles the case where we have a locally generated block with
    // nothing in the recv_blocks vector
    //
    // XXX/demmer this needs some options for the router to select
    // which block elements should be in the list, i.e. security, etc
    for (int i = 0; i < 256; ++i) {
        BlockProcessor* bp = find_processor(i);
        if (bp == UnknownBlockProcessor::instance()) {
            continue;
        }

        if (! xmit_blocks->has_block(i)) {
            bp->prepare(bundle, xmit_blocks, NULL, link, BlockInfo::LIST_NONE);
        }
    }

    // include any metadata locally generated by the API or DP
    BlockProcessor* metadata_processor = find_processor(METADATA_BLOCK);
    ASSERT(metadata_processor != NULL);
    ASSERT(metadata_processor->block_type() == METADATA_BLOCK);
    ((MetadataBlockProcessor *)metadata_processor)->
        prepare_generated_metadata(bundle, xmit_blocks, link);

#ifdef BSP_ENABLED
    // Finally add security blocks
    SPD::prepare_out_blocks(bundle, link, xmit_blocks);
#endif

    return xmit_blocks;
}

//----------------------------------------------------------------------
size_t
BundleProtocol::generate_blocks(Bundle*        bundle,
                                BlockInfoVec*  blocks,
                                const LinkRef& link)
{
    // now assert there's at least 2 blocks (primary + payload) and
    // that the primary is first
    ASSERT(blocks->size() >= 2);
    ASSERT(blocks->front().type() == PRIMARY_BLOCK);

    // now we make another pass through the list and call generate on
    // each block processor
    BlockInfoVec::iterator last_block = blocks->end() - 1;
    for (BlockInfoVec::iterator iter = blocks->begin();
         iter != blocks->end();
         ++iter)
    {
        bool last = (iter == last_block);
        iter->owner()->generate(bundle, blocks, &*iter, link, last);

        log_debug_p(LOG, "generated block (owner 0x%x type 0x%x) "
                    "data_offset %u data_length %u contents length %zu",
                    iter->owner()->block_type(), iter->type(),
                    iter->data_offset(), iter->data_length(),
                    iter->contents().len());
        
        if (last) {
            ASSERT((iter->flags() & BLOCK_FLAG_LAST_BLOCK) != 0);
        } else {
            ASSERT((iter->flags() & BLOCK_FLAG_LAST_BLOCK) == 0);
        }
    }
    
    // Now that all the EID references are added to the dictionary,
    // generate the primary block.
    PrimaryBlockProcessor* pbp =
        (PrimaryBlockProcessor*)find_processor(PRIMARY_BLOCK);
    ASSERT(blocks->front().owner() == pbp);
    pbp->generate_primary(bundle, blocks, &blocks->front());
    
    // make a final pass through, calling finalize() and extracting
    // the block length
    //
    // NOTE: this pass must be in reverse order, from end of the list
    // to the begining, in order for security processing to work.
    size_t total_len = 0;
    for (BlockInfoVec::reverse_iterator iter = blocks->rbegin();
         iter != blocks->rend();
         ++iter)
    {
        iter->owner()->finalize(bundle, blocks, &*iter, link);
        total_len += iter->full_length();
    }
    
    return total_len;
}

//----------------------------------------------------------------------
void
BundleProtocol::delete_blocks(Bundle* bundle, const LinkRef& link)
{
    ASSERT(bundle != NULL);

    bundle->xmit_blocks_.delete_blocks(link);

    BlockProcessor* metadata_processor = find_processor(METADATA_BLOCK);
    ASSERT(metadata_processor != NULL);
    ASSERT(metadata_processor->block_type() == METADATA_BLOCK);
    ((MetadataBlockProcessor *)metadata_processor)->
                                   delete_generated_metadata(bundle, link);
}

//----------------------------------------------------------------------
size_t
BundleProtocol::total_length(const BlockInfoVec* blocks)
{
    size_t ret = 0;
    for (BlockInfoVec::const_iterator iter = blocks->begin();
         iter != blocks->end();
         ++iter)
    {
        ret += iter->full_length();
    }

    return ret;
}

//----------------------------------------------------------------------
size_t
BundleProtocol::payload_offset(const BlockInfoVec* blocks)
{
    size_t ret = 0;
    for (BlockInfoVec::const_iterator iter = blocks->begin();
         iter != blocks->end();
         ++iter)
    {
        if (iter->type() == PAYLOAD_BLOCK) {
            ret += iter->data_offset();
            return ret;
        }

        ret += iter->full_length();
    }

    return ret;
}

//----------------------------------------------------------------------
size_t
BundleProtocol::produce(const Bundle* bundle, const BlockInfoVec* blocks,
                        u_char* data, size_t offset, size_t len, bool* last)
{
    size_t origlen = len;
    *last = false;

    if (len == 0)
        return 0;
    
    // advance past any blocks that are skipped by the given offset
    ASSERT(!blocks->empty());
    BlockInfoVec::const_iterator iter = blocks->begin();
    while (offset >= iter->full_length()) {
        log_debug_p(LOG, "BundleProtocol::produce skipping block type 0x%x "
                    "since offset %zu >= block length %u",
                    iter->type(), offset, iter->full_length());
        
        offset -= iter->full_length();
        iter++;
        ASSERT(iter != blocks->end());
    }
    
    // the offset should be within the bounds of this block
    ASSERT(iter != blocks->end());
        
    // figure out the amount to generate from this block
    while (1) {
        size_t remainder = iter->full_length() - offset;
        size_t tocopy    = std::min(len, remainder);
        log_debug_p(LOG, "BundleProtocol::produce copying %zu/%zu bytes from "
                    "block type 0x%x at offset %zu",
                    tocopy, remainder, iter->type(), offset);
        iter->owner()->produce(bundle, &*iter, data, offset, tocopy);
        
        len    -= tocopy;
        data   += tocopy;
        offset = 0;

        // if we've copied out the full amount the user asked for,
        // we're done. note that we need to check the corner case
        // where we completed the block exactly to properly set the
        // *last bit
        if (len == 0) {
            if ((tocopy == remainder) &&
                (iter->flags() & BLOCK_FLAG_LAST_BLOCK))
            {
                ASSERT(iter + 1 == blocks->end());
                *last = true;
            }
            
            break;
        }

        // we completed the current block, so we're done if this
        // is the lat block, even if there's space in the user buffer
        ASSERT(tocopy == remainder);
        if (iter->flags() & BLOCK_FLAG_LAST_BLOCK) {
            ASSERT(iter + 1 == blocks->end());
            *last = true;
            break;
        }
        
        ++iter;
        ASSERT(iter != blocks->end());
    }
    
    log_debug_p(LOG, "BundleProtocol::produce complete: "
                "produced %zu bytes, bundle %s",
                origlen - len, *last ? "complete" : "not complete");
    
    return origlen - len;
}
    
//----------------------------------------------------------------------
int
BundleProtocol::consume(Bundle* bundle,
                        u_char* data,
                        size_t  len,
                        bool*   last)
{
    size_t origlen = len;
    *last = false;
    
    // special case for first time we get called, since we need to
    // create a BlockInfo struct for the primary block without knowing
    // the typecode or the length
    if (bundle->recv_blocks_.empty()) {
        log_debug_p(LOG, "consume: got first block... "
                    "creating primary block info");
        bundle->recv_blocks_.append_block(find_processor(PRIMARY_BLOCK));
    }

    // loop as long as there is data left to process
    while (len != 0) {
        log_debug_p(LOG, "consume: %zu bytes left to process", len);
        BlockInfo* info = &bundle->recv_blocks_.back();

        // if the last received block is complete, create a new one
        // and push it onto the vector. at this stage we consume all
        // blocks, even if there's no BlockProcessor that understands
        // how to parse it
        if (info->complete()) {
            info = bundle->recv_blocks_.append_block(find_processor(*data));
            log_debug_p(LOG, "consume: previous block complete, "
                        "created new BlockInfo type 0x%x",
                        info->owner()->block_type());
        }
        
        // now we know that the block isn't complete, so we tell it to
        // consume a chunk of data
        log_debug_p(LOG, "consume: block processor 0x%x type 0x%x incomplete, "
                    "calling consume (%zu bytes already buffered)",
                    info->owner()->block_type(), info->type(),
                    info->contents().len());
        
        int cc = info->owner()->consume(bundle, info, data, len);
        if (cc < 0) {
            log_err_p(LOG, "consume: protocol error handling block 0x%x",
                      info->type());
            return -1;
        }
        
        // decrement the amount that was just handled from the overall
        // total. verify that the block was either completed or
        // consumed all the data that was passed in.
        len  -= cc;
        data += cc;

        log_debug_p(LOG, "consume: consumed %u bytes of block type 0x%x (%s)",
                    cc, info->type(),
                    info->complete() ? "complete" : "not complete");

        if (info->complete()) {
            // check if we're done with the bundle
            if (info->flags() & BLOCK_FLAG_LAST_BLOCK) {
                *last = true;
                break;
            }
                
        } else {
            ASSERT(len == 0);
        }
    }
    
    log_debug_p(LOG, "consume completed, %zu/%zu bytes consumed %s",
                origlen - len, origlen, *last ? "(completed bundle)" : "");
    return origlen - len;
}

//----------------------------------------------------------------------
bool
BundleProtocol::validate(Bundle* bundle,
                         status_report_reason_t* reception_reason,
                         status_report_reason_t* deletion_reason)
{
    int primary_blocks = 0, payload_blocks = 0;
    BlockInfoVec* recv_blocks = &bundle->recv_blocks_;
 
    // a bundle must include at least two blocks (primary and payload)
    if (recv_blocks->size() < 2) {
        log_err_p(LOG, "bundle fails to contain at least two blocks");
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    // the first block of a bundle must be a primary block
    if (recv_blocks->front().type() != BundleProtocol::PRIMARY_BLOCK) {
        log_err_p(LOG, "bundle fails to contain a primary block");
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    // validate each individual block
    BlockInfoVec::iterator last_block = recv_blocks->end() - 1;
    for (BlockInfoVec::iterator iter = recv_blocks->begin();
         iter != recv_blocks->end();
         ++iter)
    {
        // a block may not have enough data for the preamble
        if (iter->data_offset() == 0) {
            // either the block is not the last one and something went
            // badly wrong, or it is the last block present
            if (iter != last_block) {
                log_err_p(LOG, "bundle block too short for the preamble");
                *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
                return false;
            }
            // this is the last block, so drop it
            log_debug_p(LOG, "forgetting preamble-starved last block");
            recv_blocks->erase(iter);
            if (recv_blocks->size() < 2) {
                log_err_p(LOG, "bundle fails to contain at least two blocks");
                *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
                return false;
            }
            // continue with the tests; results may have changed now that
            // a different block is last
            iter = last_block = recv_blocks->end() - 1;
        }
        else {
            if (iter->type() == BundleProtocol::PRIMARY_BLOCK) {
                primary_blocks++;
            }

            if (iter->type() == BundleProtocol::PAYLOAD_BLOCK) {
                payload_blocks++;
            }
        }

        if (!iter->owner()->validate(bundle, recv_blocks, &*iter, 
                                reception_reason, deletion_reason)) {
            return false;
        }

        // a bundle's last block must be flagged as such,
        // and all other blocks should not be flagged
        if (iter == last_block) {
            if (!iter->last_block()) {
                // this may be okay, if it is a reactive fragment
                if (!bundle->fragmented_incoming_) {
                    log_err_p(LOG, "bundle's last block not flagged");
                    *deletion_reason
                        = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
                    return false;
                }
                else {
                    log_debug_p(LOG, "bundle's last block not flagged, but "
                                     "it is a reactive fragment");
                }
            }
        } else {
            if (iter->last_block()) {
                log_err_p(LOG, "bundle block incorrectly flagged as last");
                *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
                return false;
            }
        }
    }

    // a bundle must contain one, and only one, primary block
    if (primary_blocks != 1) {
        log_err_p(LOG, "bundle contains %d primary blocks", primary_blocks);
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }
           
    // a bundle must not contain more than one payload block
    if (payload_blocks > 1) {
        log_err_p(LOG, "bundle contains %d payload blocks", payload_blocks);
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

#ifdef BSP_ENABLED
    // verify that bundle matches inbound security policy
    // XXX also need to verify that block order is sane!
    if (! SPD::verify_in_policy(bundle)) {
        log_err_p(LOG, "bundle failed security policy verification");
        *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
        return false;
    }
#endif

    return true;
}

//----------------------------------------------------------------------
int
BundleProtocol::set_timestamp(u_char* ts, size_t len, const BundleTimestamp* tv)
{
    int sec_size = SDNV::encode(tv->seconds_, ts, len);
    if (sec_size < 0)
        return -1;

    int seqno_size = SDNV::encode(tv->seqno_, ts + sec_size, len - sec_size);
    if (seqno_size < 0)
        return -1;
    
    return sec_size + seqno_size;
}

//----------------------------------------------------------------------
int
BundleProtocol::get_timestamp(BundleTimestamp* tv, const u_char* ts, size_t len)
{
    u_int64_t tmp;
    
    int sec_size = SDNV::decode(ts, len, &tmp);
    if (sec_size < 0)
        return -1;
    ASSERT(tmp < 0xffffffff);
    tv->seconds_ = (u_int32_t)tmp;
    
    int seqno_size = SDNV::decode(ts + sec_size, len - sec_size, &tmp);
    if (seqno_size < 0)
        return -1;
    ASSERT(tmp < 0xffffffff);
    tv->seqno_ = (u_int32_t)tmp;
    
    return sec_size + seqno_size;
}

size_t
BundleProtocol::ts_encoding_len(const BundleTimestamp* tv)
{
    return SDNV::encoding_len(tv->seconds_) + SDNV::encoding_len(tv->seqno_);    
}

//----------------------------------------------------------------------
bool
BundleProtocol::get_admin_type(const Bundle* bundle, admin_record_type_t* type)
{
    if (! bundle->is_admin_) {
        return false;
    }

    u_char buf[16];
    const u_char* bp = bundle->payload_.read_data(0, sizeof(buf), buf);

    switch (bp[0] >> 4)
    {
#define CASE(_what) case _what: *type = _what; return true;

        CASE(ADMIN_STATUS_REPORT);
        CASE(ADMIN_CUSTODY_SIGNAL);
        CASE(ADMIN_ANNOUNCE);

#undef  CASE
    default:
        return false; // unknown type
    }

    NOTREACHED;
}

} // namespace dtn

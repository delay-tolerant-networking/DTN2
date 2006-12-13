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

namespace dtn {

static const char* LOG = "/dtn/bundle/protocol";

//////////////////////////////////////////////////////////////////////

// XXX NEW INTERFACE

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
}

//----------------------------------------------------------------------
BlockInfoVec*
BundleProtocol::prepare_blocks(Bundle* bundle, Link* link)
{
    // create a new block list for the outgoing link by first calling
    // prepare on all the BlockProcessor classes for the blocks that
    // arrived on the link
    BlockInfoVec* xmit_blocks = bundle->xmit_blocks_.create_blocks(link);
    BlockInfoVec* recv_blocks = &bundle->recv_blocks_;

    // if there is a received block, the first one better be the primary
    if (recv_blocks->size() > 0) {
        ASSERT(recv_blocks->front().owner()->block_type() == PRIMARY_BLOCK);
    }
    
    for (BlockInfoVec::iterator iter = recv_blocks->begin();
         iter != recv_blocks->end();
         ++iter)
    {
        iter->owner()->prepare(bundle, link, xmit_blocks, &*iter);
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
            bp->prepare(bundle, link, xmit_blocks, NULL);
        }
    }

    return xmit_blocks;
}

//----------------------------------------------------------------------
size_t
BundleProtocol::generate_blocks(Bundle*       bundle,
                                BlockInfoVec* blocks,
                                Link*         link)
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
        iter->owner()->generate(bundle, link, &*iter, last);

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
    
    // make a final pass through, calling finalize() and extracting
    // the block length
    size_t total_len = 0;
    for (BlockInfoVec::iterator iter = blocks->begin();
         iter != blocks->end();
         ++iter)
    {
        iter->owner()->finalize(bundle, link, &*iter);
        total_len += iter->full_length();
    }
    
    return total_len;
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
        if (iter->owner()->block_type() == PAYLOAD_BLOCK) {
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
        bundle->recv_blocks_.push_back(BlockInfo(find_processor(PRIMARY_BLOCK)));
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
            bundle->recv_blocks_.push_back(BlockInfo(find_processor(*data)));
            info = &bundle->recv_blocks_.back();
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
                    info->complete() ? "complete" : "not completE");

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
    if (!recv_blocks->front().primary_block()) {
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
	if (iter->primary_block()) {
            primary_blocks++;
	}

        if (iter->payload_block()) {
            payload_blocks++;
        }

        if (!iter->owner()->validate(bundle, &*iter, reception_reason,
                                                     deletion_reason)) {
            return false;
        }

        // a bundle's last block must be flagged as such,
        // and all other blocks should not be flagged
	if (iter == last_block) {
            if (!iter->last_block()) {
                log_err_p(LOG, "bundle's last block not flagged");
                *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
                return false;
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

    return true;
}

//////////////////////////////////////////////////////////////////////
//
// OLD INTERFACE: only supports primary and payload

//----------------------------------------------------------------------
int
BundleProtocol::format_header_blocks(const Bundle* bundle,
                                     u_char* buf, size_t len)
{
    BlockProcessor* primary_bp = find_processor(PRIMARY_BLOCK);
    BlockProcessor* payload_bp = find_processor(PAYLOAD_BLOCK);
    
    BlockInfo primary(primary_bp);
    BlockInfo payload(payload_bp);

    primary_bp->generate(bundle, NULL, &primary, false);
    payload_bp->generate(bundle, NULL, &payload, true);

    size_t primary_len = primary.full_length();
    ASSERT(primary_len == primary.contents().len());
    ASSERT(primary_len == primary.data_length());
    ASSERT(primary_len == primary.full_length());
    
    size_t payload_hdr_len = payload.data_offset();
    ASSERT(payload_hdr_len == payload.contents().len());
    ASSERT(payload_hdr_len <= payload.full_length());

    if (primary_len + payload_hdr_len > len) {
        log_debug_p(LOG, "format_header_blocks: "
                    "need %zu bytes primary + %zu payload_hdr, only have %zu",
                    primary_len, payload_hdr_len, len);
        return -1; // too short
    }
    
    primary_bp->produce(bundle, &primary, buf, 0, primary_len);
    payload_bp->produce(bundle, &payload, buf + primary_len, 0, payload_hdr_len);
    
    log_debug_p(LOG, "format_header_blocks done -- total length %zu",
                (primary_len + payload_hdr_len));
    return primary_len + payload_hdr_len;
}

//----------------------------------------------------------------------
int
BundleProtocol::parse_header_blocks(Bundle* bundle,
                                    u_char* buf, size_t len)
{
    size_t origlen = len;

    BlockProcessor* primary_bp = find_processor(PRIMARY_BLOCK);
    BlockProcessor* payload_bp = find_processor(PAYLOAD_BLOCK);

    ASSERT(bundle->recv_blocks_.empty());
    bundle->recv_blocks_.push_back(BlockInfo(primary_bp));
    bundle->recv_blocks_.push_back(BlockInfo(payload_bp));

    BlockInfo& primary = *(bundle->recv_blocks_.begin());
    BlockInfo& payload = *(bundle->recv_blocks_.begin() + 1);

    int primary_len = primary_bp->consume(bundle, &primary, buf, len);

    if (primary_len == -1) {
        log_err_p(LOG, "protocol error parsing primary");
        bundle->recv_blocks_.clear();
        return -1;
    }
    
    if (!primary.complete()) {
        log_debug_p(LOG, "buffer too short to consume primary");
        bundle->recv_blocks_.clear();
        return -1;
    }

    ASSERT(primary_len <= (int)len);
    ASSERT(primary_len == (int)primary.full_length());
    ASSERT(primary_len == (int)primary.contents().len());

    buf += primary_len;
    len -= primary_len;

    int payload_hdr_len = payload_bp->consume_preamble(&payload, buf, len);

    if (payload_hdr_len == -1) {
        log_err_p(LOG, "protocol error parsing payload");
        bundle->recv_blocks_.clear();
        return -1;
    }
    
    if (payload.data_offset() == 0) {
        log_debug_p(LOG, "buffer too short to consume primary");
        bundle->recv_blocks_.clear();
        return -1;
    }

    len -= payload_hdr_len;

    bundle->payload_.set_length(payload.data_length());
    
    // that's all we parse, return the amount we consumed
    log_debug_p(LOG, "parse_header_blocks succeeded: "
                "%d primary %d payload header",
                primary_len, payload_hdr_len);
    
    return origlen - len;
}

//----------------------------------------------------------------------
size_t
BundleProtocol::header_block_length(const Bundle* bundle)
{
    return PrimaryBlockProcessor::get_primary_len(bundle) +
        sizeof(BlockPreamble) +
        SDNV::encoding_len(bundle->payload_.length());
}

//----------------------------------------------------------------------
size_t
BundleProtocol::formatted_length(const Bundle* bundle)
{
    return header_block_length(bundle) +
        bundle->payload_.length() +
        tail_block_length(bundle);
}

//----------------------------------------------------------------------
int
BundleProtocol::format_bundle(const Bundle* bundle, u_char* buf, size_t len)
{
    size_t origlen = len;
    
    // first the header blocks
    int ret = format_header_blocks(bundle, buf, len);
    if (ret < 0) {
        return ret;
    }
    buf += ret;
    len -= ret;

    // then the payload
    size_t payload_len = bundle->payload_.length();
    if (payload_len > len) {
        return -1;
    }
    bundle->payload_.read_data(0, payload_len, buf,
                               BundlePayload::FORCE_COPY);
    len -= payload_len;
    buf += payload_len;

    ret = format_tail_blocks(bundle, buf, len);
    if (ret < 0) {
        return ret;
    }
    len -= ret;
    buf += ret;

    return origlen - len;
}
    
//----------------------------------------------------------------------
int
BundleProtocol::parse_bundle(Bundle* bundle, u_char* buf, size_t len)
{
    size_t origlen = len;
    
    // first the header blocks
    int ret = parse_header_blocks(bundle, buf, len);
    if (ret < 0) {
        return ret;
    }
    buf += ret;
    len -= ret;

    // then the payload
    size_t payload_len = bundle->payload_.length();
    if (payload_len > len) {
        return -1;
    }
    bundle->payload_.set_data(buf, payload_len);
    len -= payload_len;
    buf += payload_len;

    ret = parse_tail_blocks(bundle, buf, len);
    if (ret < 0) {
        return ret;
    }
    len -= ret;
    buf += ret;

    return origlen - len;
}
    
//----------------------------------------------------------------------
void
BundleProtocol::set_timestamp(u_char* ts, const BundleTimestamp* tv)
{
    u_int32_t tmp;

    tmp = htonl(tv->seconds_);
    memcpy(ts, &tmp, sizeof(u_int32_t));
    ts += sizeof(u_int32_t);

    tmp = htonl(tv->seqno_);
    memcpy(ts, &tmp, sizeof(u_int32_t));
}

//----------------------------------------------------------------------
void
BundleProtocol::get_timestamp(BundleTimestamp* tv, const u_char* ts)
{
    u_int32_t tmp;

    memcpy(&tmp, ts, sizeof(u_int32_t));
    tv->seconds_  = ntohl(tmp);
    ts += sizeof(u_int32_t);
    
    memcpy(&tmp, ts, sizeof(u_int32_t));
    tv->seqno_ = ntohl(tmp);
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

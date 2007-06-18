/*
 *    Copyright 2006 Intel Corporation
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

#include <oasys/debug/Log.h>

#include "BlockProcessor.h"
#include "BlockInfo.h"
#include "Bundle.h"
#include "SDNV.h"

namespace dtn {

//----------------------------------------------------------------------
BlockProcessor::BlockProcessor(int block_type)
    : block_type_(block_type)
{
}

//----------------------------------------------------------------------
BlockProcessor::~BlockProcessor()
{
}

//----------------------------------------------------------------------
int
BlockProcessor::consume_preamble(BlockInfo* block,
                                 u_char*    buf,
                                 size_t     len,
                                 u_int64_t* flagp)
{
    static const char* log = "/dtn/bundle/protocol";
    int sdnv_len;
    ASSERT(! block->complete());
    ASSERT(block->data_offset() == 0);

    // Since we need to be able to handle a preamble that's split
    // across multiple calls, we proactively copy up to the maximum
    // length of the preamble into the contents buffer, then adjust
    // the length of the buffer accordingly.
    size_t max_preamble  = BundleProtocol::PREAMBLE_FIXED_LENGTH +
            SDNV::MAX_LENGTH * 2;
    size_t prev_consumed = block->contents().len();
    size_t tocopy        = std::min(len, max_preamble - prev_consumed);
    
    ASSERT(max_preamble > prev_consumed);
    
    // The block info buffer must already contain enough space for the
    // preamble in the static part of the scratch buffer
    BlockInfo::DataBuffer* contents = block->writable_contents();
    ASSERT(contents->nfree() >= tocopy);
    memcpy(contents->end(), buf, tocopy);
    contents->set_len(contents->len() + tocopy);

    // Make sure we have at least one byte of sdnv before trying to
    // parse it.
    if (contents->len() <= BundleProtocol::PREAMBLE_FIXED_LENGTH) {
        ASSERT(tocopy == len);
        return len;
    }
    
    size_t buf_offset = BundleProtocol::PREAMBLE_FIXED_LENGTH;
    u_int64_t flags;
    
    // Now we try decoding the sdnv that contains the block processing
    // flags. If we can't, then we have a partial preamble, so we can
    // assert that the whole incoming buffer was consumed.
    sdnv_len = SDNV::decode(contents->buf() + buf_offset,
                            contents->len() - buf_offset,
                            &flags);
    if (sdnv_len == -1) {
        ASSERT(tocopy == len);
        return len;
    }
    
    if (flagp != NULL)
        *flagp = flags;
    
    buf_offset += sdnv_len;
    
    // Now we try decoding the sdnv that contains the actual block
    // length. If we can't, then we have a partial preamble, so we can
    // assert that the whole incoming buffer was consumed.
    u_int64_t block_len;
    sdnv_len = SDNV::decode(contents->buf() + buf_offset,
                            contents->len() - buf_offset,
                            &block_len);
    if (sdnv_len == -1) {
        ASSERT(tocopy == len);
        return len;
    }

    if (block_len > 0xFFFFFFFFLL) {
        // XXX/demmer implement big blocks
        log_err_p(log, "overflow in SDNV value for block type 0x%x",
                  *contents->buf());
        return -1;
    }
    
    buf_offset += sdnv_len;

    // We've successfully consumed the preamble so initialize the
    // data_length and data_offset fields of the block and adjust the
    // length field of the contents buffer to include only the
    // preamble part (even though a few more bytes might be in there.
    block->set_data_length(static_cast<u_int32_t>(block_len));
    block->set_data_offset(buf_offset);
    contents->set_len(buf_offset);

    log_debug_p(log, "BlockProcessor type 0x%x "
                "consumed preamble %zu/%u for block: "
                "data_offset %u data_length %u",
                block_type(), buf_offset + prev_consumed,
                block->full_length(),
                block->data_offset(), block->data_length());
    
    // Finally, be careful to return only the amount of the buffer
    // that we needed to complete the preamble.
    ASSERT(buf_offset > prev_consumed);
    return buf_offset - prev_consumed;
}

//----------------------------------------------------------------------
void
BlockProcessor::generate_preamble(BlockInfo* block,
                                  u_int8_t   type,
                                  u_int64_t  flags,
                                  u_int64_t  data_length)
{
    static const char* log = "/dtn/bundle/protocol";
    (void)log;
    
    size_t flag_sdnv_len = SDNV::encoding_len(flags);
    size_t length_sdnv_len = SDNV::encoding_len(data_length);
    ASSERT(block->contents().len() == 0);
    ASSERT(block->contents().buf_len() >= BundleProtocol::PREAMBLE_FIXED_LENGTH 
            + flag_sdnv_len + length_sdnv_len);

    u_char* bp = block->writable_contents()->buf();
    
    *bp = type;
    SDNV::encode(flags, bp + BundleProtocol::PREAMBLE_FIXED_LENGTH,
                 flag_sdnv_len);
    SDNV::encode(data_length, bp + BundleProtocol::PREAMBLE_FIXED_LENGTH + 
                 flag_sdnv_len, length_sdnv_len);

    block->set_data_length(data_length);
    u_int32_t offset = BundleProtocol::PREAMBLE_FIXED_LENGTH + 
            flag_sdnv_len + length_sdnv_len;
    block->set_data_offset(offset);
    block->writable_contents()->set_len(offset);

    log_debug_p(log, "BlockProcessor type 0x%x "
                "generated preamble for block type 0x%x flags 0x%llx: "
                "data_offset %u data_length %u",
                block_type(), block->type(), U64FMT(block->flags()),
                block->data_offset(), block->data_length());
}

//----------------------------------------------------------------------
int
BlockProcessor::consume(Bundle* bundle, BlockInfo* block,
                        u_char* buf, size_t len)
{
    (void)bundle;
    
    static const char* log = "/dtn/bundle/protocol";
    (void)log;
    
    size_t consumed = 0;

    ASSERT(! block->complete());

    // Check if we still need to consume the preamble by checking if
    // the data_offset_ field is initialized in the block info
    // structure.
    if (block->data_offset() == 0) {
        int cc = consume_preamble(block, buf, len);
        if (cc == -1) {
            return -1;
        }

        buf += cc;
        len -= cc;

        consumed += cc;
    }

    // If we still don't know the data offset, we must have consumed
    // the whole buffer
    if (block->data_offset() == 0) {
        ASSERT(len == 0);
    }

    // If the preamble is complete (i.e., data offset is non-zero) and
    // the block's data length is zero, then mark the block as complete
    if (block->data_offset() != 0 && block->data_length() == 0) {
        block->set_complete(true);
    }
    
    // If there's nothing left to do, we can bail for now.
    if (len == 0)
        return consumed;

    // Now make sure there's still something left to do for the block,
    // otherwise it should have been marked as complete
    ASSERT(block->data_length() == 0 ||
           block->full_length() > block->contents().len());

    // make sure the contents buffer has enough space
    block->writable_contents()->reserve(block->full_length());

    size_t rcvd      = block->contents().len();
    size_t remainder = block->full_length() - rcvd;
    size_t tocopy;

    if (len >= remainder) {
        block->set_complete(true);
        tocopy = remainder;
    } else {
        tocopy = len;
    }
    
    // copy in the data
    memcpy(block->writable_contents()->end(), buf, tocopy);
    block->writable_contents()->set_len(rcvd + tocopy);
    len -= tocopy;
    consumed += tocopy;

    log_debug_p(log, "BlockProcessor type 0x%x "
                "consumed %zu/%u for block type 0x%x (%s)",
                block_type(), consumed, block->full_length(), block->type(),
                block->complete() ? "complete" : "not complete");
    
    return consumed;
}

//----------------------------------------------------------------------
bool
BlockProcessor::validate(const Bundle* bundle, BlockInfo* block,
                    BundleProtocol::status_report_reason_t* reception_reason,
                    BundleProtocol::status_report_reason_t* deletion_reason)
{
    static const char * log = "/dtn/bundle/protocol";
    (void)reception_reason;

    // An administrative bundle MUST NOT contain an extension block
    // with a processing flag that requires a reception status report
    // be transmitted in the case of an error
    if (bundle->is_admin_ &&
        block->type() != BundleProtocol::PRIMARY_BLOCK &&
        block->flags() & BundleProtocol::BLOCK_FLAG_REPORT_ONERROR) {
        log_err_p(log, "invalid block flag 0x%x for received admin bundle",
                  BundleProtocol::BLOCK_FLAG_REPORT_ONERROR);
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }
        
    return true;
}

//----------------------------------------------------------------------
void
BlockProcessor::prepare(const Bundle*    bundle,
                        const LinkRef&   link,
                        BlockInfoVec*    blocks,
                        const BlockInfo* source)
{
    (void)bundle;
    (void)link;
    blocks->push_back(BlockInfo(this, source));
}

//----------------------------------------------------------------------
void
BlockProcessor::finalize(const Bundle*  bundle,
                         const LinkRef& link,
                         BlockInfo*     block)
{
    (void)link;
        
    if (bundle->is_admin_ && block->type() != BundleProtocol::PRIMARY_BLOCK) {
        ASSERT((block->flags() &
                BundleProtocol::BLOCK_FLAG_REPORT_ONERROR) == 0);
    }
}

//----------------------------------------------------------------------
void
BlockProcessor::produce(const Bundle* bundle, const BlockInfo* block,
                        u_char* buf, size_t offset, size_t len)
{
    (void)bundle;
    ASSERT(offset < block->contents().len());
    ASSERT(block->contents().len() >= offset + len);
    memcpy(buf, block->contents().buf() + offset, len);
}

//----------------------------------------------------------------------
void
BlockProcessor::init_block(BlockInfo* block, u_int8_t type, u_int8_t flags,
                           const u_char* bp, size_t len)
{
    ASSERT(block->owner() != NULL);
    generate_preamble(block, type, flags, len);
    ASSERT(block->data_offset() != 0);
    block->writable_contents()->reserve(block->full_length());
    block->writable_contents()->set_len(block->full_length());
    memcpy(block->writable_contents()->buf() + block->data_offset(),
           bp, len);
}

} // namespace dtn

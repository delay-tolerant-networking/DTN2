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

#include <oasys/debug/Log.h>

#include "BlockProcessor.h"
#include "BlockInfo.h"
#include "BundleProtocol.h"
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
                                 size_t     preamble_size)
{
    static const char* log = "/dtn/bundle/protocol";
    ASSERT(! block->complete());
    ASSERT(block->data_offset() == 0);

    if (preamble_size == 0) {
        preamble_size = sizeof(BundleProtocol::BlockPreamble);
    }
    
    // Since we need to be able to handle a preamble that's split
    // across multiple calls, we proactively copy up to the maximum
    // length of the preamble into the contents buffer, then adjust
    // the length of the buffer accordingly.
    size_t max_preamble  = preamble_size + SDNV::MAX_LENGTH;
    size_t prev_consumed = block->contents().len();
    size_t tocopy        = std::min(len, max_preamble - prev_consumed);
    
    ASSERT(max_preamble > prev_consumed);
    
    // The block info buffer must already contain enough space for the
    // preamble in the static part of the scratch buffer
    BlockInfo::DataBuffer* contents = block->writable_contents();
    ASSERT(contents->nfree() >= tocopy);
    memcpy(contents->end(), buf, tocopy);
    contents->set_len(contents->len() + tocopy);
    
    // Now we try decoding the sdnv that contains the actual block
    // length. If we can't, then we have a partial preamble, so we can
    // assert that the whole incoming buffer was consumed.
    u_int64_t block_len;
    int sdnv_len =
        SDNV::decode(contents->buf() + preamble_size,
                     contents->len() - preamble_size,
                     &block_len);
    if (sdnv_len == -1) {
        ASSERT(tocopy == len);
        return len;
    }

    if (block_len > 0xFFFFFFFFLL) {
        // XXX/demmer implement big blocks
        log_err(log, "overflow in SDNV value for block type 0x%x", *contents->buf());
        return -1;
    }

    // We've successfully consumed the preamble so initialize the
    // data_length and data_offset fields of the block and adjust the
    // length field of the contents buffer to include only the
    // preamble part (even though a few more bytes might be in there.
    block->set_data_length(static_cast<u_int32_t>(block_len));
    block->set_data_offset(preamble_size + sdnv_len);
    contents->set_len(preamble_size + sdnv_len);

    log_debug(log, "BlockProcessor consumed preamble %zu/%u for block type 0x%x",
              preamble_size + sdnv_len - prev_consumed,
              block->full_length(), block->type());
    
    // Finally, be careful to return only the amount of the buffer
    // that we needed to complete the preamble.
    ASSERT(preamble_size + sdnv_len > prev_consumed);
    return preamble_size + sdnv_len - prev_consumed;
}

//----------------------------------------------------------------------
void
BlockProcessor::generate_preamble(BlockInfo* block,
                                  u_int8_t   type,
                                  u_int8_t   flags,
                                  size_t     data_length)
{
    size_t sdnv_len = SDNV::encoding_len(data_length);
    ASSERT(block->contents().len() == 0);
    ASSERT(block->contents().buf_len() >=
           sizeof(BundleProtocol::BlockPreamble) + sdnv_len);

    BundleProtocol::BlockPreamble* bp =
        (BundleProtocol::BlockPreamble*)block->writable_contents()->buf();

    bp->type  = type;
    bp->flags = flags;
    SDNV::encode(data_length, &bp->length[0], sdnv_len);

    block->set_data_length(data_length);
    block->set_data_offset(sizeof(*bp) + sdnv_len);
    block->writable_contents()->set_len(sizeof(*bp) + sdnv_len);
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
    
    // If there's nothing left to do, we can bail for now.
    if (len == 0)
        return consumed;
    
    // Now make sure there's still something left to do for the block,
    // otherwise it should have been marked as complete
    ASSERT(block->data_length() > block->contents().len());

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

    log_debug(log, "BlockProcessor consumed %zu/%u for block type 0x%x (%s)",
              consumed, block->full_length(), block->type(),
              block->complete() ? "complete" : "not complete");
    
    return consumed;
}

//----------------------------------------------------------------------
bool
BlockProcessor::validate(const Bundle* bundle, BlockInfo* block)
{
    (void)bundle;
    (void)block;
    return true;
}

//----------------------------------------------------------------------
void
BlockProcessor::prepare(const Bundle* bundle, LinkBlocks* blocks)
{
    (void)bundle;
    blocks->info_vec_.push_back(BlockInfo(this));
}

//----------------------------------------------------------------------
void
BlockProcessor::finalize(const Bundle* bundle, Link* link, BlockInfo* block)
{
    (void)bundle;
    (void)link;
    (void)block;
}

//----------------------------------------------------------------------
void
BlockProcessor::produce(const Bundle* bundle, BlockInfo* block,
                        u_char* buf, size_t offset, size_t len)
{
    (void)bundle;
    ASSERT(block->contents().len() >= offset + len);
    memcpy(buf, block->contents().buf() + offset, len);
}


} // namespace dtn

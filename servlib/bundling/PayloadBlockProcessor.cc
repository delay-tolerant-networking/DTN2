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

#include "PayloadBlockProcessor.h"
#include "Bundle.h"
#include "BundleProtocol.h"

namespace dtn {

//----------------------------------------------------------------------
PayloadBlockProcessor::PayloadBlockProcessor()
    : BlockProcessor(BundleProtocol::PAYLOAD_BLOCK)
{
}

//----------------------------------------------------------------------
int
PayloadBlockProcessor::consume(Bundle*    bundle,
                               BlockInfo* block,
                               u_char*    buf,
                               size_t     len)
{
    static const char* log = "/dtn/bundle/protocol";
    (void)log;
    
    size_t consumed = 0;
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
    
    // Otherwise, the buffer should hold just the preamble
    ASSERT(block->contents().len() == block->data_offset());

    // Now make sure there's still something left to do for the block,
    // otherwise it should have been marked as complete
    ASSERT(block->data_length() > bundle->payload_.length());

    size_t rcvd      = bundle->payload_.length();
    size_t remainder = block->data_length() - rcvd;
    size_t tocopy;

    if (len >= remainder) {
        block->set_complete(true);
        tocopy = remainder;
    } else {
        tocopy = len;
    }

    bundle->payload_.set_length(rcvd + tocopy);
    bundle->payload_.reopen_file();
    bundle->payload_.write_data(buf, rcvd, tocopy);
    bundle->payload_.close_file();

    consumed += tocopy;

    log_debug(log, "PayloadBlockProcessor consumed %zu/%u (%s)",
              consumed, block->full_length(), 
              block->complete() ? "complete" : "not complete");
    
    return consumed;
}

//----------------------------------------------------------------------
void
PayloadBlockProcessor::generate(const Bundle* bundle,
                                Link*         link,
                                BlockInfo*    block,
                                bool          last)
{
    (void)link;
    
    // in the ::generate pass, we just need to set up the preamble,
    // since the payload stays on disk
    generate_preamble(block,
                      BundleProtocol::PAYLOAD_BLOCK,
                      last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0,
                      bundle->payload_.length());
}

//----------------------------------------------------------------------
void
PayloadBlockProcessor::produce(const Bundle* bundle, BlockInfo* block,
                               u_char* buf, size_t offset, size_t len)
{
    // First copy out the specified range of the preamble
    if (offset < block->data_offset()) {
        size_t tocopy = std::min(len, block->data_offset() - offset);
        memcpy(buf, block->contents().buf() + offset, tocopy);
        buf    += tocopy;
        offset += tocopy;
        len    -= tocopy;
    }

    if (len == 0)
        return;

    // Adjust offset to account for the preamble
    size_t payload_offset = offset - block->data_offset();

    bundle->payload_.read_data(payload_offset, len, buf,
                               BundlePayload::FORCE_COPY);
}

} // namespace dtn

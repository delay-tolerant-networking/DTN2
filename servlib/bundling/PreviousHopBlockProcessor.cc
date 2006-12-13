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

#include "PreviousHopBlockProcessor.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocol.h"
#include "contacts/Link.h"

namespace dtn {

//----------------------------------------------------------------------
PreviousHopBlockProcessor::PreviousHopBlockProcessor()
    : BlockProcessor(BundleProtocol::PREVIOUS_HOP_BLOCK)
{
}

//----------------------------------------------------------------------
void
PreviousHopBlockProcessor::prepare(const Bundle*    bundle,
                                   Link*            link,
                                   BlockInfoVec*    blocks,
                                   const BlockInfo* source)
{
    if (link == NULL || !link->params().prevhop_hdr_) {
        return;
    }
    
    BlockProcessor::prepare(bundle, link, blocks, source);
}

//----------------------------------------------------------------------
void
PreviousHopBlockProcessor::generate(const Bundle* bundle,
                                    Link*         link,
                                    BlockInfo*    block,
                                    bool          last)
{
    (void)bundle;
    (void)link;

    // XXX/demmer this is not the right protocol spec'd format since
    // it's supposed to use the dictionary
    

    // if the flag isn't set, we shouldn't have a block
    ASSERT(link->params().prevhop_hdr_);

    BundleDaemon* bd = BundleDaemon::instance();
    size_t length = bd->local_eid().length();
    
    generate_preamble(block,
                      BundleProtocol::PREVIOUS_HOP_BLOCK,
                      BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |
                        (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
                      length);

    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->data_offset() + length);
    contents->set_len(block->data_offset() + length);
    memcpy(contents->buf() + block->data_offset(),
           bd->local_eid().data(), length);
}

//----------------------------------------------------------------------
int
PreviousHopBlockProcessor::consume(Bundle* bundle, BlockInfo* block,
                                   u_char* buf, size_t len)
{
    int cc = BlockProcessor::consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

    if (! bundle->prevhop_.assign((char*)block->data(), block->data_length())) {
        log_err_p("/dtn/bundle/protocol",
                  "error parsing previous hop eid '%.*s",
                  block->data_length(), block->data());
        return -1;
    }
    
    return cc;
}


} // namespace dtn

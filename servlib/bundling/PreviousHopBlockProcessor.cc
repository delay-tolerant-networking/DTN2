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
#  include <dtn-config.h>
#endif

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
int
PreviousHopBlockProcessor::prepare(const Bundle*    bundle,
                                   BlockInfoVec*    xmit_blocks,
                                   const BlockInfo* source,
                                   const LinkRef&   link,
                                   list_owner_t     list)
{
    if (link == NULL || !link->params().prevhop_hdr_) {
        return BP_FAIL;
    }
    
    return BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
}

//----------------------------------------------------------------------
int
PreviousHopBlockProcessor::generate(const Bundle*  bundle,
                                    BlockInfoVec*  xmit_blocks,
                                    BlockInfo*     block,
                                    const LinkRef& link,
                                    bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;

    // XXX/demmer this is not the right protocol spec'd format since
    // it's supposed to use the dictionary
    

    // if the flag isn't set, we shouldn't have a block
    ASSERT(link->params().prevhop_hdr_);

    BundleDaemon* bd = BundleDaemon::instance();
    size_t length = bd->local_eid().length();
    
    generate_preamble(xmit_blocks, 
                      block,
                      BundleProtocol::PREVIOUS_HOP_BLOCK,
                      BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |
                        (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
                      length);

    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->data_offset() + length);
    contents->set_len(block->data_offset() + length);
    memcpy(contents->buf() + block->data_offset(),
           bd->local_eid().data(), length);

    return BP_SUCCESS;
}

//----------------------------------------------------------------------
int
PreviousHopBlockProcessor::consume(Bundle*    bundle,
                                   BlockInfo* block,
                                   u_char*    buf,
                                   size_t     len)
{
    int cc = BlockProcessor::consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

    if (! bundle->mutable_prevhop()->
        assign((char*)block->data(), block->data_length()))
    {
        log_err_p("/dtn/bundle/protocol",
                  "error parsing previous hop eid '%.*s",
                  block->data_length(), block->data());
        return -1;
    }
    
    return cc;
}

//----------------------------------------------------------------------
int
PreviousHopBlockProcessor::format(oasys::StringBuffer* buf)
{
	buf->append("Previous Hop Block");
}


} // namespace dtn

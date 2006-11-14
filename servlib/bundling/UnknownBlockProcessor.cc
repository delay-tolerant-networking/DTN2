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

#include "UnknownBlockProcessor.h"

#include "BlockInfo.h"
#include "BundleProtocol.h"

namespace dtn {

template <> UnknownBlockProcessor*
oasys::Singleton<UnknownBlockProcessor>::instance_ = NULL;

//----------------------------------------------------------------------
UnknownBlockProcessor::UnknownBlockProcessor()
    : BlockProcessor(0xff) // typecode is ignored for this processor
{
}

//----------------------------------------------------------------------
void
UnknownBlockProcessor::generate(const Bundle* bundle,
                                Link*         link,
                                BlockInfo*    block,
                                bool          last)
{
    (void)bundle;
    (void)link;
    
    // This can only be called if there was a corresponding block in
    // the input path
    const BlockInfo* source = block->source();
    ASSERT(source != NULL);
    ASSERT(source->owner() == this);

    // We shouldn't be here if the block has the following flags set
    ASSERT((source->flags() &
            BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR) == 0);
    ASSERT((source->flags() &
            BundleProtocol::BLOCK_FLAG_DISCARD_BLOCK_ONERROR) == 0);
    
    // The source better have some contents, but doesn't need to have
    // any data necessarily
    ASSERT(source->contents().len() != 0);
    ASSERT(source->data_offset() != 0);
    
    u_int8_t flags = source->flags();
    if (last) {
        flags |= BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
    } else {
        flags &= ~BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
    }

    generate_preamble(block, source->type(), flags,
                      source->data_length());
    ASSERT(block->data_offset() == source->data_offset());
    ASSERT(block->data_length() == source->data_length());
    
    BlockInfo::DataBuffer* contents = block->writable_contents();
    memcpy(contents->buf()          + block->data_offset(),
           source->contents().buf() + block->data_offset(),
           block->data_length());
    contents->set_len(block->full_length());
}

} // namespace dtn
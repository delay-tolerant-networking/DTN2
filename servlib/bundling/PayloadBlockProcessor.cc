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
    NOTREACHED;

    // XXX/demmer this should handle the actual payload eventually
    BlockProcessor::consume(bundle, block, buf, len);
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

} // namespace dtn

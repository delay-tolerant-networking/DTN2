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

#include "BundleProtocol.h"
#include "UnknownBlockProcessor.h"

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
    (void)block;
    (void)last;

    // XXX/demmer this should copy the block from the corresponding
    // one on the input side
    NOTREACHED;
}

} // namespace dtn

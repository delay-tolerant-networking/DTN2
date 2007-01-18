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

#ifndef _PAYLOAD_BLOCK_PROCESSOR_H_
#define _PAYLOAD_BLOCK_PROCESSOR_H_

#include "BlockProcessor.h"

namespace dtn {

/**
 * Block processor implementation for the payload bundle block.
 */
class PayloadBlockProcessor : public BlockProcessor {
public:
    /// Constructor
    PayloadBlockProcessor();
    
    /// @{ Virtual from BlockProcessor
    int consume(Bundle* bundle, BlockInfo* block, u_char* buf, size_t len);
    void generate(const Bundle* bundle, const LinkRef& link,
                  BlockInfo* block, bool last);
    void produce(const Bundle* bundle, const BlockInfo* block,
                 u_char* buf, size_t offset, size_t len);
    /// @}
};

} // namespace dtn

#endif /* _PAYLOAD_BLOCK_PROCESSOR_H_ */

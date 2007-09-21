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
    int generate(const Bundle* bundle, BlockInfoVec* xmit_blocks,
                  BlockInfo* block, const LinkRef& link, bool last);
    bool validate(const Bundle* bundle, BlockInfoVec*  block_list, BlockInfo* block,
                  BundleProtocol::status_report_reason_t* reception_reason,
                  BundleProtocol::status_report_reason_t* deletion_reason);
    void produce(const Bundle* bundle, const BlockInfo* block,
                 u_char* buf, size_t offset, size_t len);
    /**
     * Accessor to virtualize processing contents of the block in various
     * ways. This is overloaded by the payload since the contents are
     * not actually stored in the BlockInfo contents_ buffer but
     * rather are on-disk.
     *
     * Processing can be anything the calling routine wishes, such as
     * digest of the block, encryption, decryption etc. This routine is
     * permitted to process the data in several calls to the target
     * "func" routine as long as the data is processed in order and 
     * exactly once. Contents of target_block may be changed and 
     * "changed" will be set to true if change occurs.
     *
     * The signature for process_func is similar but not identical.
     *
     * Note that the supplied offset + length must be less than or
     * equal to the total length of the block.
     */
    virtual void     process(const Bundle* bundle,
                             const BlockInfo* caller_block,
                             const BlockInfo* target_block,
                             process_func_const* func, 
                             size_t offset, 
                             size_t& len,
                             OpaqueContext* r);

    virtual void     process(Bundle* bundle,
                             const BlockInfo* caller_block,
                             BlockInfo* target_block,
                             process_func* func, 
                             size_t offset, 
                             size_t& len,
                             OpaqueContext* r,
                             bool& changed);

    /// @}
};

} // namespace dtn

#endif /* _PAYLOAD_BLOCK_PROCESSOR_H_ */

/*
 *    Copyright 2006 SPARTA Inc
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

#ifndef _CIPHERSUITE_ES4_H_
#define _CIPHERSUITE_ES4_H_

#ifdef BSP_ENABLED

#include <oasys/util/ScratchBuffer.h>
#include "gcm/gcm_aes.h"
#include "gcm/gcm.h"
#include "bundling/BlockProcessor.h"
#include "ES_BlockProcessor.h"

namespace dtn {


typedef oasys::ScratchBuffer<u_char*, 64> DataBuffer;


/**
 * Block processor implementation for the extension security block.
 */
class Ciphersuite_ES4 : public Ciphersuite {
public:
    enum {
        op_invalid = 0,
        op_encrypt = 1,
        op_decrypt = 2
    };

    typedef struct {
        u_int8_t operation;
        gcm_ctx  c;
    } gcm_ctx_ex;

    /// Constructor
    Ciphersuite_ES4();
    
    virtual u_int16_t cs_num();
    
    /// @{ Virtual from BlockProcessor
    /**
     * First callback for parsing blocks that is expected to append a
     * chunk of the given data to the given block. When the block is
     * completely received, this should also parse the block into any
     * fields in the bundle class.
     *
     * The base class implementation parses the block preamble fields
     * to find the length of the block and copies the preamble and the
     * data in the block's contents buffer.
     *
     * This and all derived implementations must be able to handle a
     * block that is received in chunks, including cases where the
     * preamble is split into multiple chunks.
     *
     * @return the amount of data consumed or -1 on error
     */
    virtual int consume(Bundle* bundle, BlockInfo* block,
                        u_char* buf, size_t len);

    /**
     * Validate the block. This is called after all blocks in the
     * bundle have been fully received.
     *
     * @return true if the block passes validation
     */
    virtual bool validate(const Bundle*           bundle,
                          BlockInfoVec*           block_list,
                          BlockInfo*              block,
                          status_report_reason_t* reception_reason,
                          status_report_reason_t* deletion_reason);

    /**
     * First callback to generate blocks for the output pass. The
     * function is expected to initialize an appropriate BlockInfo
     * structure in the given BlockInfoVec.
     *
     * The base class simply initializes an empty BlockInfo with the
     * appropriate owner_ pointer.
     */
    virtual int prepare(const Bundle*    bundle,
                        BlockInfoVec*    xmit_blocks,
                        const BlockInfo* source,
                        const LinkRef&   link,
                        list_owner_t     list);
    
    /**
     * Second callback for transmitting a bundle. This pass should
     * generate any data for the block that does not depend on other
     * blocks' contents.
     */
    virtual int generate(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link,
                         bool           last);
    
    /**
     * Third callback for transmitting a bundle. This pass should
     * generate any data (such as security signatures) for the block
     * that may depend on other blocks' contents.
     *
     * The base class implementation does nothing. 
     */
    virtual int finalize(const Bundle*  bundle, 
                         BlockInfoVec*  xmit_blocks, 
                         BlockInfo*     block, 
                         const LinkRef& link);

    /**
     * Callback for encrypt/decrypt a block. This is normally
     * used for handling the payload contents.
     */
    static bool do_crypt(const Bundle*    bundle,
                         const BlockInfo* caller_block,
                         BlockInfo*       target_block,
                         void*            buf,
                         size_t           len,
                         OpaqueContext*   r);

    /**
     * Test if there's still an extension block (called by finalize)
     */
    bool contains_extension_block(BlockInfoVec*  xmit_blocks);

    /**
     * Test if there's still an ESB block (called by verify)
     */
    BlockInfo* contains_ESB_block(BlockInfoVec* block_list);


    /**
     * Ciphersuite number
     *   iv_len is only 8 for GCM, which also uses 4-byte nonce
     */
    enum { CSNUM_ES4 = 4, 
           key_len   = 128/8, 
           nonce_len = 12,
           salt_len  = 4, 
           iv_len    = nonce_len - salt_len, 
           tag_len   = 128/8
    };
    

    /// @}
};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _CIPHERSUITE_ES4_H_ */

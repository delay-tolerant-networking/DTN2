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

#ifndef _BLOCKPROCESSOR_H_
#define _BLOCKPROCESSOR_H_

#include <oasys/compat/inttypes.h>

#include "BundleProtocol.h"
#include "BlockInfo.h"
#include "BP_Local.h"

namespace dtn {

class BlockInfo;
class BlockInfoVec;
class Bundle;

/**
 * Base class for the protocol handling of bundle blocks, including
 * the core primary and payload handling, security, and other
 * extension blocks.
 */
class BlockProcessor {
public:
    /**
     * Constructor that takes the block typecode. Generally, typecodes
     * should be defined in BundleProtocol::bundle_block_type_t, but
     * the field is defined as an int so that handlers for
     * non-specified blocks can be defined.
     */
    BlockProcessor(int block_type);
                   
    /**
     * Virtual destructor.
     */
    virtual ~BlockProcessor();
    
    /// @{ Accessors
    int block_type() { return block_type_; }
    /// @}
    
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
    virtual bool validate(const Bundle* bundle, BlockInfo* block,
                     BundleProtocol::status_report_reason_t* reception_reason,
                     BundleProtocol::status_report_reason_t* deletion_reason);

    /**
     * First callback to generate blocks for the output pass. The
     * function is expected to initialize an appropriate BlockInfo
     * structure in the given BlockInfoVec.
     *
     * The base class simply initializes an empty BlockInfo with the
     * appropriate owner_ pointer.
     */
    virtual void prepare(const Bundle*    bundle,
                         const LinkRef&   link,
                         BlockInfoVec*    xmit_blocks,
                         BlockInfoVec*    source_blocks,
                         const BlockInfo* source,
                         BlockInfo::list_owner_t list);
    
    /**
     * Second callback for transmitting a bundle. This pass should
     * generate any data for the block that does not depend on other
     * blocks' contents.  It MUST add any EID references it needs by
     * calling block->add_eid(), then call generate_preamble(), which
     * will add the EIDs to the primary block's dictionary and write
     * their offsets to this block's preamble.
     */
    virtual void generate(const Bundle*  bundle,
                          const LinkRef& link,
                          BlockInfoVec*  xmit_blocks,
                          BlockInfo*     block,
                          bool           last) = 0;
    
    /**
     * Third callback for transmitting a bundle. This pass should
     * generate any data (such as security signatures) for the block
     * that may depend on other blocks' contents.
     *
     * The base class implementation does nothing. 
     * 
     * We pass xmit_blocks explicitly to indicate that ALL blocks
     * might be changed by finalize, typically by being encrypted.
     * Parameters such as length might also change due to padding
     * and encapsulation.
     */
    virtual void finalize(const Bundle*  bundle, 
                          const LinkRef& link, 
                          BlockInfoVec*  xmit_blocks, 
                          BlockInfo*     block);

    /**
     * Parse a block preamble consisting of type, flags(SDNV),
     * EID-list (composite field of SDNVs) and length(SDNV).
     * This method does not apply to the primary block, but is
     * suitable for payload and all extensions.
     * The parse does NOT modify the BlockInfo fields, as 
     * consume_preamble() does. This is a convenience routine
     * for extension code, such as bundle security,  which must
     * process blocks owned by other extensions.
    int parse_preamble(BlockInfo* block,
                         u_char* buf,
                         size_t len,
                         u_int64_t* flagp = NULL);
     */

    /**
     * Accessor to virtualize copying contents out from the block
     * info. This is overloaded by the payload since the contents are
     * not actually stored in the BlockInfo contents_ buffer but
     * rather are on-disk.
     *
     * The base class implementation simply does a memcpy from the
     * contents into the supplied buffer.
     *
     * Note that the supplied offset + length must be less than or
     * equal to the total length of the block.
     */
    virtual void produce(const Bundle* bundle, const BlockInfo* block,
                         u_char* buf, size_t offset, size_t len);

    /**
     * General hook to set up a block with the given contents. Used
     * for testing generic extension blocks.
     */
    void init_block(BlockInfo* block, u_int8_t type, u_int8_t flags,
                    const u_char* bp, size_t len);
    
protected:
    friend class BundleProtocol;
    friend class BlockInfo;
    
    /**
     * Consume a block preamble consisting of type, flags(SDNV),
     * EID-list (composite field of SDNVs) and length(SDNV).
     * This method does not apply to the primary block, but is
     * suitable for payload and all extensions.
     */
    int consume_preamble(BlockInfoVec*  recv_blocks, 
                         BlockInfo* block,
                         u_char* buf,
                         size_t len,
                         u_int64_t* flagp = NULL);
    
    /**
     * Generate the standard preamble for the given block type, flags,
     * EID-list and content length.
     */
    void generate_preamble(BlockInfoVec*  xmit_blocks, 
                           BlockInfo* block,
                           u_int8_t type,
                           u_int64_t flags,
                           u_int64_t data_length);
    
private:
    /// The block typecode for this handler
    int block_type_;
};

} // namespace dtn

#endif /* _BLOCKPROCESSOR_H_ */

/*
 *    Copyright 2010-2011 Trinity College Dublin
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

#ifdef BPQ_ENABLED

#include "BPQBlockProcessor.h"

namespace dtn {

// Setup our logging information
static const char* LOG = "/dtn/bundle/extblock/bpq";

template <> BPQBlockProcessor*
oasys::Singleton<BPQBlockProcessor>::instance_ = NULL;



//----------------------------------------------------------------------
BPQBlockProcessor::BPQBlockProcessor() :
        BlockProcessor(BundleProtocol::QUERY_EXTENSION_BLOCK)
{
    log_info_p(LOG, "BPQBlockProcessor::BPQBlockProcessor()");
}

//----------------------------------------------------------------------
int
BPQBlockProcessor::consume(Bundle* bundle,
                           BlockInfo* block,
                           u_char* buf,
                           size_t len)
{
    log_info_p(LOG, "BPQBlockProcessor::consume() start");

    int cc;

    if ( (cc = BlockProcessor::consume(bundle, block, buf, len)) < 0) {
        log_err_p(LOG, "BPQBlockProcessor::consume(): error handling block 0x%x",
                        BundleProtocol::QUERY_EXTENSION_BLOCK);
        return cc;
    }

    // If we don't finish processing the block, return the number of bytes
    // consumed. (Error checking done in the calling function?)
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

    BPQBlock* bpq_block = new BPQBlock(bundle);
    log_info_p(LOG, "     BPQBlock:");
    log_info_p(LOG, "         kind: %d", bpq_block->kind());
    log_info_p(LOG, "matching rule: %d", bpq_block->matching_rule());
    log_info_p(LOG, "    query_len: %d", bpq_block->query_len());
    log_info_p(LOG, "    query_val: %s", bpq_block->query_val());
    delete bpq_block;

    log_info_p(LOG, "BPQBlockProcessor::consume() end");
    
    return cc;
}

//----------------------------------------------------------------------

int 
BPQBlockProcessor::prepare(const Bundle*    bundle,
                           BlockInfoVec*    xmit_blocks,
                           const BlockInfo* source,
                           const LinkRef&   link,
                           list_owner_t     list)
{
    log_info_p(LOG, "BPQBlockProcessor::prepare()");

    (void)bundle;
    (void)link;
    (void)list; 

    log_debug_p(LOG, "prepare(): data_length() = %u", source->data_length());
    log_debug_p(LOG, "prepare(): data_offset() = %u", source->data_offset());
    log_debug_p(LOG, "prepare(): full_length() = %u", source->full_length());

    // Received blocks are added to the end of the list (which
    // maintains the order they arrived in) but API blocks 
    // are added after the primary block (that is, before the
    // payload and the received blocks). This places them "outside"
    // the original blocks.

    if ( list == BlockInfo::LIST_API ) {
        log_info_p(LOG, "Adding BPQBlock found in API Block Vec to xmit_blocks");

        ASSERT((*xmit_blocks)[0].type() == BundleProtocol::PRIMARY_BLOCK);
        xmit_blocks->insert(xmit_blocks->begin() + 1, BlockInfo(this, source));
        ASSERT(xmit_blocks->has_block(BundleProtocol::QUERY_EXTENSION_BLOCK));

        return BP_SUCCESS;

    } else if ( list == BlockInfo::LIST_RECEIVED ) {

        log_info_p(LOG, "Adding BPQBlock found in Recv Block Vec to xmit_blocks");

        xmit_blocks->append_block(this, source);
        ASSERT(xmit_blocks->has_block(BundleProtocol::QUERY_EXTENSION_BLOCK));

        return BP_SUCCESS;


    } else {

        log_err_p(LOG, "BPQBlock not found in bundle");
        return BP_FAIL;
    }
}

//----------------------------------------------------------------------
int
BPQBlockProcessor::generate(const Bundle*  bundle,
                            BlockInfoVec*  xmit_blocks,
                            BlockInfo*     block,
                            const LinkRef& link,
                            bool           last)
{
    log_info_p(LOG, "BPQBlockProcessor::generate() starting");

    (void)xmit_blocks;    
    (void)link;

    ASSERT (block->type() == BundleProtocol::QUERY_EXTENSION_BLOCK);

    // set flags
    u_int8_t flags = BundleProtocol::BLOCK_FLAG_REPLICATE |
                     (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0);
                     //BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |

    BlockInfo* bpq_info;

    if ( (const_cast<Bundle*>(bundle))->api_blocks()->
            has_block(BundleProtocol::QUERY_EXTENSION_BLOCK) ) {

        bpq_info = const_cast<BlockInfo*>((const_cast<Bundle*>(bundle))->
                   api_blocks()->find_block(BundleProtocol::QUERY_EXTENSION_BLOCK));
        log_info_p(LOG, "BPQBlock found in API Block Vec => created locally");
        
    } else if ( (const_cast<Bundle*>(bundle))->recv_blocks().
                has_block(BundleProtocol::QUERY_EXTENSION_BLOCK) ) {


        bpq_info = const_cast<BlockInfo*>((const_cast<Bundle*>(bundle))->
                   recv_blocks().find_block(BundleProtocol::QUERY_EXTENSION_BLOCK));
        log_info_p(LOG, "BPQBlock found in Recv Block Vec => created remotly");

    } else {
        log_err_p(LOG, "Cannot find BPQ block");
        return BP_FAIL;
    }

    BPQBlock* bpq_block = new BPQBlock(bundle);

    int length = bpq_block->length();
  //int length = bpq_info->data_length();

    generate_preamble(xmit_blocks,
                      block,
                      BundleProtocol::QUERY_EXTENSION_BLOCK,
                      flags,
                      length ); 


    // The process of storing the value into the block. We'll create a
    // `DataBuffer` object and `reserve` the length of our BPQ data and
    // update the length of the `DataBuffer`.

    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->data_offset() + length);
    contents->set_len(block->data_offset() + length);

    // Set our pointer to the right offset.
    u_char* buf = contents->buf() + block->data_offset();
    
    // now write contents of BPQ block into the block
    if ( bpq_block->write_to_buffer(buf, length) == -1 ) {
        log_err_p(LOG, "Error writing BPQ block to buffer");
        return BP_FAIL;
    }

    delete bpq_block;
    log_info_p(LOG, "BPQBlockProcessor::generate() ending");
    return BP_SUCCESS;
}
//----------------------------------------------------------------------
bool
BPQBlockProcessor::validate(const Bundle*           bundle,
                            BlockInfoVec*           block_list,
                            BlockInfo*              block,
                            status_report_reason_t* reception_reason,
                            status_report_reason_t* deletion_reason)
{
    if ( ! BlockProcessor::validate(bundle, 
                                    block_list,
                                    block,reception_reason,
                                    deletion_reason) ) {
        log_err_p(LOG, "BlockProcessor validation failed");
        return false;
    }
    
    if ( block->data_offset() + block->data_length() != block->full_length() ) {
        
        log_err_p(LOG, "offset (%u) + data len (%u) is not equal to the full len (%u)",
                  block->data_offset(), block->data_length(), block->full_length() );
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    if ( block->contents().buf_len() < block->full_length() ) {

        log_err_p(LOG, "block buffer len (%u) is less than the full len (%u)",
                  block->contents().buf_len(), block->full_length() );
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    if ( *(block->data()) != 0 && *(block->data()) != 1 ) {

        log_err_p(LOG, "invalid kind - should be query (0) or response (1) but is: %u",
                  (u_int) *block->data() );
        *deletion_reason = BundleProtocol::REASON_BLOCK_UNINTELLIGIBLE;
        return false;
    }

    return true;
}

} // namespace dtn

#endif /* BPQ_ENABLED */

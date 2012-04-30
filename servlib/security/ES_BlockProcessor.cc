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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include "ES_BlockProcessor.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "contacts/Link.h"
#include "openssl/evp.h"

namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
ES_BlockProcessor::ES_BlockProcessor()
    : BlockProcessor(BundleProtocol::EXTENSION_SECURITY_BLOCK)
{
}

//----------------------------------------------------------------------
int
ES_BlockProcessor::consume(Bundle* bundle, BlockInfo* block,
                          u_char* buf, size_t len)
{
    int cc = BlockProcessor::consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    // in on-the-fly scenario, process this data for those interested
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

    if ( block->locals() == NULL ) {      // then we need to parse it
        Ciphersuite::parse(block);
    }
    
    return cc;
}

//----------------------------------------------------------------------
int
ES_BlockProcessor::reload_post_process(Bundle*       bundle,
                                      BlockInfoVec* block_list,
                                      BlockInfo*    block)
{

    // Received blocks might be stored and reloaded and
    // some fields aren't reset.
    // This allows BlockProcessors to reestablish what they
    // need
    
    Ciphersuite* p = NULL;
    int          err = 0;
    int          type = 0;
    BP_Local_CS* locals;
    
    if ( ! block->reloaded() )
        return 0;
        
    type = block->type();
    log_debug_p(log, "ES_BlockProcessor::reload block type %d", type);
    
    Ciphersuite::parse(block);
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);

    p = Ciphersuite::find_suite( locals->owner_cs_num() );
    if ( p != NULL ) 
        err = p->reload_post_process(bundle, block_list, block);
    
    block->set_reloaded(false);
    return err;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(Ciphersuite::CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
bool
ES_BlockProcessor::validate(const Bundle*           bundle,
                           BlockInfoVec*           block_list,
                           BlockInfo*              block,
                           status_report_reason_t* reception_reason,
                           status_report_reason_t* deletion_reason)
{
    (void)bundle;
    (void)block_list;
    (void)block;
    (void)reception_reason;
    (void)deletion_reason;

    Ciphersuite* p = NULL;
    u_int16_t    cs_flags = 0;
    EndpointID   local_eid = BundleDaemon::instance()->local_eid();
    BP_Local_CS* locals = dynamic_cast<BP_Local_CS*>(block->locals());
    bool         result = false;

    CS_FAIL_IF_NULL(locals);


    log_debug_p(log, "ES_BlockProcessor::validate() %p ciphersuite %d",
                block, locals->owner_cs_num());
    cs_flags = locals->cs_flags();
    
    if ( Ciphersuite::destination_is_local_node(bundle, block) )
    {  //yes - this is ours 
        
        p = Ciphersuite::find_suite( locals->owner_cs_num() );
        if ( p != NULL ) {
            result = p->validate(bundle, block_list, block,
                                 reception_reason, deletion_reason);
            return result;
        } else {
            log_err_p(log, "block failed security validation ES_BlockProcessor");
            *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
            return false;
        }
    } else {
        // not for here so we didn't check this block
        locals->set_proc_flag(Ciphersuite::CS_BLOCK_DID_NOT_FAIL);   
    }

    return true;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(Ciphersuite::CS_BLOCK_FAILED_VALIDATION |
                              Ciphersuite::CS_BLOCK_COMPLETED_DO_NOT_FORWARD);
    *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
    return false;
}

//----------------------------------------------------------------------
int
ES_BlockProcessor::prepare(const Bundle*    bundle,
                          BlockInfoVec*    xmit_blocks,
                          const BlockInfo* source,
                          const LinkRef&   link,
                          list_owner_t     list)
{
    (void)bundle;
    (void)link;
    (void)list;

    Ciphersuite*    p = NULL;
    int             result = BP_FAIL;
    BP_Local_CS*    locals = NULL;
    BP_Local_CS*    source_locals = NULL;

    if ( list == BlockInfo::LIST_RECEIVED ) {
        

// XXX/pl  review this to see how much is actually needed


        ASSERT(source != NULL);
        u_int16_t       cs_flags = 0;

        if ( Ciphersuite::destination_is_local_node(bundle, source) )
            return BP_SUCCESS;     //don't forward if it's for here

        xmit_blocks->push_back(BlockInfo(this, source));
        BlockInfo* bp = &(xmit_blocks->back());
        bp->set_eid_list(source->eid_list());
        log_debug_p(log, "ES_BlockProcessor::prepare() - forward received block len %u",
                    source->full_length());
        

        CS_FAIL_IF_NULL( source->locals() );      // broken

        source_locals = dynamic_cast<BP_Local_CS*>(source->locals());
        CS_FAIL_IF_NULL(source_locals);    
        bp->set_locals(new BP_Local_CS);
        locals = dynamic_cast<BP_Local_CS*>(bp->locals());
        CS_FAIL_IF_NULL(locals);
        locals->set_owner_cs_num(source_locals->owner_cs_num());
        cs_flags = source_locals->cs_flags();
        locals->set_correlator(source_locals->correlator());
        locals->set_list_owner(BlockInfo::LIST_RECEIVED);
        
        // copy security-src and -dest if they exist
        if ( source_locals->cs_flags() & Ciphersuite::CS_BLOCK_HAS_SOURCE ) {
            ASSERT(source_locals->security_src().length() > 0 );
            cs_flags |= Ciphersuite::CS_BLOCK_HAS_SOURCE;
            locals->set_security_src(source_locals->security_src());
            log_debug_p(log, "ES_BlockProcessor::prepare() add security_src EID %s", 
                        source_locals->security_src().c_str());
        }
        
        if ( source_locals->cs_flags() & Ciphersuite::CS_BLOCK_HAS_DEST ) {
            ASSERT(source_locals->security_dest().length() > 0 );
            cs_flags |= Ciphersuite::CS_BLOCK_HAS_DEST;
            locals->set_security_dest(source_locals->security_dest());
            log_debug_p(log, "ES_BlockProcessor::prepare() add security_dest EID %s",
                        source_locals->security_dest().c_str());
        }
        locals->set_cs_flags(cs_flags);
        log_debug_p(log, "ES_BlockProcessor::prepare() - inserted block eid_list_count %zu",
                    bp->eid_list().size());
        result = BP_SUCCESS;
    } else {
        if ( source != NULL ) {
            source_locals = dynamic_cast<BP_Local_CS*>(source->locals());
            CS_FAIL_IF_NULL(source_locals);    
            p = Ciphersuite::find_suite( source_locals->owner_cs_num() );
            if ( p != NULL ) {
                result = p->prepare(bundle, xmit_blocks, source, link, list);
            } else {
                log_err_p(log, "ES_BlockProcessor::prepare() - ciphersuite %d is missing",
                          source_locals->owner_cs_num());
            }
        }  // no msg if "source" is NULL, as BundleProtocol calls all BPs that way once
    }
    return result;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(Ciphersuite::CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
ES_BlockProcessor::generate(const Bundle*  bundle,
                           BlockInfoVec*  xmit_blocks,
                           BlockInfo*     block,
                           const LinkRef& link,
                           bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;

    Ciphersuite*    p = NULL;
    int             result = BP_FAIL;

    log_debug_p(log, "ES_BlockProcessor::generate()");
    
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);

    p = Ciphersuite::find_suite( locals->owner_cs_num() );
    if ( p != NULL ) {
        result = p->generate(bundle, xmit_blocks, block, link, last);
    } else {
        // generate the preamble and copy the data.
        size_t length = block->source()->data_length();
        
        generate_preamble(xmit_blocks, 
                          block,
                          BundleProtocol::CONFIDENTIALITY_BLOCK,
                          //BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |  //This causes non-BSP nodes to delete the bundle
                          (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
                          length);

        BlockInfo::DataBuffer* contents = block->writable_contents();
        contents->reserve(block->data_offset() + length);
        contents->set_len(block->data_offset() + length);
        memcpy(contents->buf() + block->data_offset(),
               block->source()->data(), length);
        result = BP_SUCCESS;
    }
    return result;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(Ciphersuite::CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
ES_BlockProcessor::finalize(const Bundle*  bundle, 
                           BlockInfoVec*  xmit_blocks,
                           BlockInfo*     block, 
                           const LinkRef& link)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    (void)block;
    
    Ciphersuite*    p = NULL;
    int             result = BP_FAIL;
    log_debug_p(log, "ES_BlockProcessor::finalize()");
    
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);

    p = Ciphersuite::find_suite( locals->owner_cs_num() );
    if ( p != NULL ) {
        result = p->finalize(bundle, xmit_blocks, block, link);
    } 
    // If we are called then it means that the ciphersuite for this Bundle
    // does not exist at this node. All the work was done in generate()
    return result;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(Ciphersuite::CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

} // namespace dtn

#endif /* BSP_ENABLED */

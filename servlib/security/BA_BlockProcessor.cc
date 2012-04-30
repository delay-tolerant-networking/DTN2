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

#include "BA_BlockProcessor.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "contacts/Link.h"

namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
BA_BlockProcessor::BA_BlockProcessor()
    : BlockProcessor(BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK)
{
}

//----------------------------------------------------------------------
int
BA_BlockProcessor::consume(Bundle* bundle, BlockInfo* block,
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
BA_BlockProcessor::reload_post_process(Bundle*       bundle,
                                       BlockInfoVec* block_list,
                                       BlockInfo*    block)
{

    // Received blocks might be stored and reloaded and
    // some fields aren't reset.
    // This allows BlockProcessors to reestablish what they
    // need
    
    Ciphersuite*    p = NULL;
    int     err = 0;
    int     type = 0;
    BP_Local_CS*    locals;
    
    if ( ! block->reloaded() )
        return 0;
        
    type = block->type();
    log_debug_p(log, "BA_BlockProcessor::reload block type %d", type);
    
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
BA_BlockProcessor::validate(const Bundle*           bundle,
                            BlockInfoVec*           block_list,
                            BlockInfo*              block,
                            status_report_reason_t* reception_reason,
                            status_report_reason_t* deletion_reason)
{
    (void)bundle;
    (void)block_list;
    (void)block;
    (void)reception_reason;

    Ciphersuite*    p = NULL;
    u_int16_t       cs_flags = 0;
    EndpointID        local_eid = BundleDaemon::instance()->local_eid();
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    bool            result = false;

    CS_FAIL_IF_NULL(locals);

    log_debug_p(log, "BA_BlockProcessor::validate() %p ciphersuite %d",
                block, locals->owner_cs_num());
    cs_flags = locals->cs_flags();
    
    p = Ciphersuite::find_suite( locals->owner_cs_num() );
    if ( p != NULL ) {
        result = p->validate(bundle, block_list, block,
                             reception_reason, deletion_reason);
    } else {
        log_err_p(log, "block failed security validation BA_BlockProcessor");
        *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
    }

    return result;


 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(Ciphersuite::CS_BLOCK_FAILED_VALIDATION |
                              Ciphersuite::CS_BLOCK_COMPLETED_DO_NOT_FORWARD);
    
    *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
    return false;
}

//----------------------------------------------------------------------
int
BA_BlockProcessor::prepare(const Bundle*    bundle,
                           BlockInfoVec*    xmit_blocks,
                           const BlockInfo* source,
                           const LinkRef&   link,
                           list_owner_t     list)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;

    Ciphersuite*    p = NULL;
    int             result = BP_FAIL;
    
    if ( list == BlockInfo::LIST_NONE || source == NULL )
        return BP_SUCCESS;
    
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(source->locals());
    CS_FAIL_IF_NULL(locals);
        
    log_debug_p(log, "BA_BlockProcessor::prepare() ciphersuite %d",
                locals->owner_cs_num());

    switch ( list ) {
    case BlockInfo::LIST_API:
    case BlockInfo::LIST_EXT:
        // we expect a specific ciphersuite, rather than the
        // generic block processor. See if we have one, and fail
        // if we do not
            
        p = Ciphersuite::find_suite(locals->owner_cs_num());
        if ( p == NULL ) {
            log_err_p(log, "BA_BlockProcessor::prepare: Couldn't find ciphersuite in registration!");
            return result;
        }
            
        // Now we know the suite, get it to prepare its own block
        result = p->prepare( bundle, xmit_blocks, source, link, list );
        if(result == BP_FAIL) {
            log_err_p(log, "BA_BlockProcessor::prepare: The ciphersuite prepare returned BP_FAIL");
        }
        break;
        
//        case BlockInfo::LIST_NONE:       //can't handle this as generic BA
//        case BlockInfo::LIST_RECEIVED:   //don't forward received BA blocks
    default:
        log_debug_p(log, "BA_BlockProcessor::prepare: We landed in the defaiult case");
        return BP_SUCCESS;
        break;
            
    }
    
    
    return result;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(Ciphersuite::CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
BA_BlockProcessor::generate(const Bundle*  bundle,
                            BlockInfoVec*  xmit_blocks,
                            BlockInfo*     block,
                            const LinkRef& link,
                            bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    (void)block;
    (void)last;

    Ciphersuite*    p = NULL;
    int             result = BP_FAIL;
    
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);

    p = Ciphersuite::find_suite( locals->owner_cs_num() );
    if ( p != NULL ) {
        result = p->generate(bundle, xmit_blocks, block, link, last);
    } else 
        log_err_p(log, "BA_BlockProcessor::generate() - ciphersuite %d is missing",
                  locals->owner_cs_num());
    
    return result;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(Ciphersuite::CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
BA_BlockProcessor::finalize(const Bundle*  bundle, 
                            BlockInfoVec*  xmit_blocks,
                            BlockInfo*     block, 
                            const LinkRef& link)
{
    (void)bundle;
    (void)xmit_blocks;
    (void)link;
    (void)block;
    
    Ciphersuite* p = NULL;
    int          result = BP_FAIL;
    
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);

    p = Ciphersuite::find_suite( locals->owner_cs_num() );
    if ( p != NULL ) {
        result = p->finalize(bundle, xmit_blocks, block, link);
    } else 
        log_err_p(log, "BA_BlockProcessor::finalize() - ciphersuite %d is missing",
                  locals->owner_cs_num());
    
    return result;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(Ciphersuite::CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

} // namespace dtn

#endif /* BSP_ENABLED */

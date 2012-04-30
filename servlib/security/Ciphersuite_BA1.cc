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

#include "Ciphersuite_BA1.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "bundling/SDNV.h"
#include "contacts/Link.h"
#include "KeyDB.h"
#include "BP_Tag.h"
#include "openssl/hmac.h"

namespace dtn {

static const char* log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
Ciphersuite_BA1::Ciphersuite_BA1()
{
}

//----------------------------------------------------------------------
u_int16_t
Ciphersuite_BA1::cs_num(void)
{
    return CSNUM_BA1;
}

//----------------------------------------------------------------------
int
Ciphersuite_BA1::consume(Bundle* bundle, BlockInfo* block,
                         u_char* buf, size_t len)
{
    log_debug_p(log, "Ciphersuite_BA1::consume()");
    int cc = block->owner()->consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    
    // in on-the-fly scenario, process this data for those interested
    
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

    if ( block->locals() == NULL ) {      // then we need to parse it
        parse(block);
    }
    
    return cc;
}

//----------------------------------------------------------------------
bool
Ciphersuite_BA1::validate(const Bundle*           bundle,
                          BlockInfoVec*           block_list,
                          BlockInfo*              block,
                          status_report_reason_t* reception_reason,
                          status_report_reason_t* deletion_reason)
{
    (void)block_list;
    size_t          offset;
    size_t          len;
    size_t          rem;
    HMAC_CTX        ctx;
    OpaqueContext*   r = reinterpret_cast<OpaqueContext*>(&ctx);
    const BlockInfoVec& recv_blocks = bundle->recv_blocks();
    u_char          result[EVP_MAX_MD_SIZE];
    u_int32_t       rlen = 0;
    BP_Local_CS*    locals = NULL;
    u_char*         buf;
    u_int64_t       cs_flags;
    u_int64_t       suite_num;
    u_int64_t       field_length           = 0LL;
    int             sdnv_len = 0;        // use an int to handle -1 return values
    (void)reception_reason;
    
    log_debug_p(log, "Ciphersuite_BA1::validate()");
    // if first block
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);
    if ( !(locals->cs_flags() & Ciphersuite::CS_BLOCK_HAS_RESULT) ) {            
        const KeyDB::Entry* key_entry =
            KeyDB::find_key(EndpointID(locals->security_src()).uri().host().c_str(), cs_num());
        if (key_entry == NULL) {
            log_warn_p(log, "unable to find verification key for this block");
            goto fail;
        }
        ASSERT(key_entry->key_len() == res_len);
        
        // dump key_entry to debugging output
//         oasys::StringBuffer ksbuf;
//         key_entry->dump(&ksbuf);
//         log_debug_p(log, "Ciphersuite_BA1::validate(): using key entry:\n%s",
//                     ksbuf.c_str());
        
        // prepare the digest context in "result"
        HMAC_CTX_init(&ctx);
        HMAC_Init_ex(&ctx, key_entry->key(), key_entry->key_len(),
                     EVP_sha1(), NULL);
        
        // walk the list and process each of the blocks
        for ( BlockInfoVec::const_iterator iter = recv_blocks.begin();
              iter != recv_blocks.end();
              ++iter)
        {
            offset = 0;
            len = iter->full_length();
            
            if ( iter->type() == BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK ) {
                // This is a BA block but might or might not be BA1.
                // So we need to see if there is a security-result field
                // which needs exclusion
                
                // ciphersuite number and flags
                u_char* ptr = iter->data();
                rem = iter->full_length();

                sdnv_len = SDNV::decode(ptr,
                                        rem,
                                        &suite_num);
                ptr += sdnv_len;
                rem -= sdnv_len;

                sdnv_len = SDNV::decode(ptr,
                                        rem,
                                        &cs_flags);
                ptr += sdnv_len;
                rem -= sdnv_len;

                if ( cs_flags & CS_BLOCK_HAS_RESULT ) {
                    // if there's a security-result we have to ease up to it
                    
                    sdnv_len =  SDNV::len(ptr);        //step over correlator
                    ptr += sdnv_len;
                    rem -= sdnv_len;
                    
                    sdnv_len =  SDNV::len(ptr);        //step over security-result-length field
                    ptr += sdnv_len;
                    rem -= sdnv_len;
                    
                    len = ptr - iter->contents().buf();  //this is the length to use
                }
            }
            
            iter->owner()->process( Ciphersuite_BA1::digest,
                                    bundle,
                                    block,
                                    &*iter,
                                    offset,
                                    len,
                                    r);
        }
        
        // finalize the digest
        HMAC_Final(&ctx, result, &rlen);
        HMAC_cleanup(&ctx);
        ASSERT(rlen == Ciphersuite_BA1::res_len);
        
        // check the digest in the result - in the *second* block
        // walk the list to find it
        for (BlockInfoVec::iterator iter = block_list->begin();
             iter != block_list->end();
             ++iter)
        {
            BP_Local_CS* target_locals;
            if ( iter->type() != BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK )
                continue;
            
            target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
            CS_FAIL_IF_NULL(target_locals);
            if ( target_locals->owner_cs_num() != CSNUM_BA1 )
                continue;
            
            if (target_locals->correlator() != locals->correlator() )
                continue;
            
            // Now we're at the block which is ...
            //   1. BA block
            //   2. BA1 ciphersuite
            //   3. same correlator as the main one        
            
            if ( target_locals->cs_flags() & Ciphersuite::CS_BLOCK_HAS_RESULT ) {
                buf = target_locals->security_result().buf();
                len = target_locals->security_result().len();
                
                // we expect only one item in the field, the BA signature
                if ( *buf++ != Ciphersuite::CS_signature_field ) {        // item type
                    log_err_p(log, "Ciphersuite_BA1 item type incorrect");
                    goto fail;                //field type is bad
                }
                len--;
                
                sdnv_len = SDNV::decode(buf, len, &field_length);        // item length
                buf += sdnv_len;
                len -= sdnv_len;
                ASSERT(field_length == Ciphersuite_BA1::res_len);
                ASSERT(         len == Ciphersuite_BA1::res_len);
                
                if ( memcmp(buf, result, Ciphersuite_BA1::res_len) != 0) {
                    log_err_p(log, "block failed security validation Ciphersuite_BA1");
                    goto fail;
                } else {
                    log_debug_p(log, "block passed security validation Ciphersuite_BA1");
                    locals->set_proc_flag(CS_BLOCK_PASSED_VALIDATION);
                    return true;
                }
            }
            else 
            {
                continue;
            }
        }
        log_err_p(log, "block failed security validation Ciphersuite_BA1 - result is missing");
        goto fail;
    }
    else    
    {
        //  do NOT set a proc_flag here, for this block as it's not the owner of the correlated set
        log_debug_p(log, "BA1BlockProcessor::validate(): no check on this block");
    }

    return true;

 fail:
    locals->set_proc_flag(CS_BLOCK_FAILED_VALIDATION | CS_BLOCK_COMPLETED_DO_NOT_FORWARD);
    *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
    return false;
}

//----------------------------------------------------------------------
int
Ciphersuite_BA1::prepare(const Bundle*    bundle,
                         BlockInfoVec*    xmit_blocks,
                         const BlockInfo* source,
                         const LinkRef&   link,
                         list_owner_t     list)
{
    (void)bundle;
    (void)link;

    int             result = BP_FAIL;
    u_int64_t       correlator = CSNUM_BA1 << 16;     //also need to add a low-order piece
    u_int16_t       flags = CS_BLOCK_HAS_CORRELATOR;
    BP_Local_CS*    locals = NULL;

    log_debug_p(log, "Ciphersuite_BA1::prepare()");
    if ( list == BlockInfo::LIST_RECEIVED )
        return BP_SUCCESS;   //don't forward received BA blocks
        
    // Need to add two blocks, one at the start, one after payload
    // It's simpler to fill in the pieces and then insert them.
    BlockInfo       bi = BlockInfo(BundleProtocol::find_processor(BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK), source);

    // initialize the first block
    BundleDaemon* bd = BundleDaemon::instance();
    bi.set_locals(new BP_Local_CS);
    locals = dynamic_cast<BP_Local_CS*>(bi.locals());
    CS_FAIL_IF_NULL(locals);
    locals->set_owner_cs_num(CSNUM_BA1);
    locals->set_cs_flags(flags | CS_BLOCK_HAS_SOURCE);
    locals->set_security_src(bd->local_eid().str());
    correlator = create_correlator(bundle, xmit_blocks);
    correlator |= (int)CSNUM_BA1 << 16;      // add our ciphersuite number
    locals->set_correlator( correlator );
    locals->set_correlator_sequence( 0 );
    
    
    // We should already have the primary block in the list.
    // If primary is there then insert after it.
    // If not, insert first in the list.
    // If list is empty then just add to back
    //   -- this will be troublesome later but we have no choice
    if ( xmit_blocks->size() > 0 ) {
        BlockInfoVec::iterator iter = xmit_blocks->begin();
        if ( iter->type() == BundleProtocol::PRIMARY_BLOCK)
            ++iter;
        xmit_blocks->insert(iter, bi);
    } else {
        xmit_blocks->push_back(bi);
    }
    
    // initialize the second (trailing) block
    bi.set_locals(new BP_Local_CS);
    locals = dynamic_cast<BP_Local_CS*>(bi.locals());
    CS_FAIL_IF_NULL(locals);
    locals->set_owner_cs_num(CSNUM_BA1);
    locals->set_cs_flags(flags | CS_BLOCK_HAS_RESULT);
    locals->set_correlator( correlator );       // same one created above, obviously
    locals->set_correlator_sequence( 1 );
    xmit_blocks->push_back(bi);

    result = BP_SUCCESS;
    return result;
    
 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
Ciphersuite_BA1::generate(const Bundle*  bundle,
                          BlockInfoVec*  xmit_blocks,
                          BlockInfo*     block,
                          const LinkRef& link,
                          bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;

    log_debug_p(log, "Ciphersuite_BA1::generate()");
    int             result = BP_FAIL;
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    u_int16_t       flags = locals->cs_flags();
    size_t          item_len = 0;
    u_char*         buf = NULL;
    int             len = 0;
    size_t          length = 0;    
    int             sdnv_len = 0;        // use an int to handle -1 return values
    BlockInfo::DataBuffer* contents = NULL;
            
    CS_FAIL_IF_NULL(locals);
    // add security-source to EID-list
    if ( flags & CS_BLOCK_HAS_SOURCE ) {
        block->add_eid(locals->security_src());
        /* xmit_blocks->dict()->add_eid() is done for us in
         * generate_preamble() below */
    }
    
    length = 0;         // ciphersuite number and flags
    length += SDNV::encoding_len(CSNUM_BA1);
    length += SDNV::encoding_len(locals->cs_flags());
    length += SDNV::encoding_len(locals->correlator());  
    
    if (flags & CS_BLOCK_HAS_RESULT) {      
        item_len = 1 + 1 + Ciphersuite_BA1::res_len; // type + length + result item
        length += SDNV::encoding_len(item_len) + item_len;
    }
    
    generate_preamble(xmit_blocks, 
                      block,
                      BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK,
                      //BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |  //This causes non-BSP nodes to delete the bundle
                      (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
                      length);

    contents = block->writable_contents();
    contents->reserve(block->data_offset() + length);
    contents->set_len(block->data_offset() + length);

    buf = contents->buf() + block->data_offset();
    len = length;
    
    // ciphersuite number and flags
    sdnv_len = SDNV::encode(CSNUM_BA1, buf, len);
    CS_FAIL_IF(sdnv_len <= 0);
    buf += sdnv_len;
    len -= sdnv_len;
    
    sdnv_len = SDNV::encode(locals->cs_flags(), buf, len);
    CS_FAIL_IF(sdnv_len <= 0);
    buf += sdnv_len;
    len -= sdnv_len;
            
    // correlator
    sdnv_len = SDNV::encode(locals->correlator(), buf, len);
    CS_FAIL_IF(sdnv_len <= 0);
    buf += sdnv_len;
    len -= sdnv_len;
    
    if (flags & CS_BLOCK_HAS_RESULT) {      
        // security-result offset
        size_t result_offset = buf - block->data();
        locals->set_security_result_offset(result_offset);
        
        // security-result length
        sdnv_len = SDNV::encode(item_len, buf, len);
        CS_FAIL_IF(sdnv_len <= 0);
        buf += sdnv_len;
        len -= sdnv_len;
    }
    CS_FAIL_IF(len != (int)item_len);
        
    result = BP_SUCCESS;
    return result;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
Ciphersuite_BA1::finalize(const Bundle*  bundle, 
                          BlockInfoVec*  xmit_blocks,
                          BlockInfo*     block, 
                          const LinkRef& link)
{
    (void)link;
    
    size_t          offset;
    size_t          len;
    size_t          rem;
    HMAC_CTX        ctx;
    OpaqueContext*   r = reinterpret_cast<OpaqueContext*>(&ctx);
    u_char          digest_result[EVP_MAX_MD_SIZE];
    u_int32_t       rlen = 0;
    int             result = BP_FAIL;
    BP_Local_CS*    locals = NULL;
    u_int64_t       cs_flags;
    u_int64_t       suite_num;
    int sdnv_len = 0;        // use an int to handle -1 return values
    log_debug_p(log, "Ciphersuite_BA1::finalize()");
    
    /* The processing for BundleAuthentication takes place
     * when finalize() is called for the "front" block, even though
     * the result itself goes into the trailing block, after the payload.
     * It is an error to calculate the digest during the finalize() call
     * for the trailing block itself, as other needed results have not
     * been created at that time. Remember that the finalize() processing
     * is a reverse iteration over all the blocks.
     */
     
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);
    if ( locals->correlator_sequence() == 0 ) {       // front block is zero
        // fetch key
        const KeyDB::Entry* key_entry = KeyDB::find_key("*", cs_num());
        // XXX/ngoffee -- fix this ASSERT later, but it's what we have
        // to do until the prepare()/generate()/finalize() interface
        // is changed to allow more subtle return codes.
        CS_FAIL_IF(key_entry == NULL);
        CS_FAIL_IF(key_entry->key_len() != res_len);
        
        // dump key_entry to debugging output
//         oasys::StringBuffer ksbuf;
//         key_entry->dump(&ksbuf);
//         log_debug_p(log, "Ciphersuite_BA1::finalize(): using key entry:\n%s",
//                     ksbuf.c_str());
        
        // prepare the digest context in "digest_result"
        HMAC_CTX_init(&ctx);
        HMAC_Init_ex(&ctx, key_entry->key(), key_entry->key_len(),
                     EVP_sha1(), NULL);
        
        // walk the list and process each of the blocks
        for (BlockInfoVec::const_iterator iter = xmit_blocks->begin();
             iter != xmit_blocks->end();
             ++iter)
        {
            offset = 0;
            len = iter->full_length();
            
            // If this is a BA block then we exclude the security result data
            // from the digest, but include its length field
            
            if ( iter->type() == BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK ) {
                // This is a BA block but might or might not be BA1.
                // So we need to see if there is a security-result field
                // which needs exclusion
                
                // ciphersuite number and flags
                u_char* ptr = iter->data();
                rem = iter->full_length();
                sdnv_len = SDNV::decode(ptr,
                                        rem,
                                        &suite_num);
                ptr += sdnv_len;
                rem -= sdnv_len;

                sdnv_len = SDNV::decode(ptr,
                                        rem,
                                        &cs_flags);
                ptr += sdnv_len;
                rem -= sdnv_len;
                
                if ( cs_flags & CS_BLOCK_HAS_RESULT ) {
                    // if there's a security-result we have to ease up to it
                    
                    sdnv_len =  SDNV::len(ptr);        //step over correlator
                    ptr += sdnv_len;
                    rem -= sdnv_len;
                    
                    sdnv_len =  SDNV::len(ptr);        //step over security-result-length field
                    ptr += sdnv_len;
                    rem -= sdnv_len;
                    
                    len = ptr - iter->contents().buf();  //this is the length to use
                }
            }
            
            iter->owner()->process( Ciphersuite_BA1::digest,
                                    bundle,
                                    block,
                                    &*iter,
                                    offset,
                                    len,
                                    r );
        }
                
        // finalize the digest
        HMAC_Final(&ctx, digest_result, &rlen);
        HMAC_cleanup(&ctx);
        CS_FAIL_IF(rlen != Ciphersuite_BA1::res_len);
        
        // place the digest into the block - it goes into the *second* block
        // walk the list to find it
        for (BlockInfoVec::iterator iter = xmit_blocks->begin();
             iter != xmit_blocks->end();
             ++iter)
        {
            BP_Local_CS* target_locals;
            if ( iter->type() != BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK )
                continue;
            
            target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
            CS_FAIL_IF_NULL(target_locals);
            if ( target_locals->owner_cs_num() != CSNUM_BA1 )
                continue;
            
            if (target_locals->correlator() != locals->correlator() )
                continue;
            
            if (target_locals->correlator_sequence() != 1 )
                continue;
            
            // Now we're at the block which is ...
            //   1. BA block
            //   2. BA1 ciphersuite
            //   3. same correlator as the main one
            //   4. correlator sequence is 1, which means second block
        
            u_char* buf = iter->writable_contents()->buf() + iter->data_offset() + target_locals->security_result_offset();
            size_t  rem = iter->data_length() - target_locals->security_result_offset();
            sdnv_len = SDNV::len(buf);            //length of security-result field
            CS_FAIL_IF(sdnv_len != 1);
            buf += sdnv_len;
            rem -= sdnv_len;
            *buf++ = Ciphersuite::CS_signature_field;                // item type
            rem--;
            sdnv_len = SDNV::encode(Ciphersuite_BA1::res_len, buf, rem);    // item length
            CS_FAIL_IF(sdnv_len != 1);
            buf += sdnv_len;
            rem -= sdnv_len;
            CS_FAIL_IF (rem != Ciphersuite_BA1::res_len);
            memcpy(buf, digest_result, Ciphersuite_BA1::res_len);
        }
    }
    
    result = BP_SUCCESS;
    return result;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
void
Ciphersuite_BA1::digest(const Bundle*    bundle,
                        const BlockInfo* caller_block,
                        const BlockInfo* target_block,
                        const void*      buf,
                        size_t           len,
                        OpaqueContext*   r)
{
    (void)bundle;
    (void)caller_block;
    (void)target_block;
    log_debug_p(log, "Ciphersuite_BA1::digest() %zu bytes", len);
    
    HMAC_CTX*       pctx = reinterpret_cast<HMAC_CTX*>(r);
    
    HMAC_Update( pctx, reinterpret_cast<const u_char*>(buf), len );
}

} // namespace dtn

#endif /* BSP_ENABLED */

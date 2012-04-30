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

#define OPENSSL_FIPS    1       /* required for sha256 */

#include "Ciphersuite_PI2.h"
#include "Ciphersuite_PC3.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "bundling/SDNV.h"
#include "contacts/Link.h"
#include "security/KeySteward.h"
#include "openssl/evp.h"

namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";

/**
 * Local definition borrowed from PrimaryBlockProcessor.h
 * and with frag_offest and orig_length added
 */
struct PrimaryBlock_ex {
    u_int8_t version;
    u_int64_t processing_flags;
    u_int64_t block_length;
    u_int64_t dest_scheme_offset;
    u_int64_t dest_ssp_offset;
    u_int64_t source_scheme_offset;
    u_int64_t source_ssp_offset;
    u_int64_t replyto_scheme_offset;
    u_int64_t replyto_ssp_offset;
    u_int64_t custodian_scheme_offset;
    u_int64_t custodian_ssp_offset;
    u_int64_t creation_time;
    u_int64_t creation_sequence;
    u_int64_t lifetime;
    u_int64_t dictionary_length;
    u_int64_t fragment_offset;
    u_int64_t original_length;
};

// Need quad versions of hton for manipulating full-length (unpacked) SDNV values

#if defined(WORDS_BIGENDIAN) && (WORDS_BIGENDIAN == 1)
#define htonq( x ) (x)
#define ntohq( x ) (x)
#else

inline u_int64_t htonq( u_int64_t x )
{
    u_int64_t   res;
    u_int32_t   hi = x >> 32;
    u_int32_t   lo = x & 0xffffffff;
    hi = htonl( hi );
    res = htonl( lo );
    res = res << 32 | hi;

    return res;
}

inline u_int64_t ntohq( u_int64_t x )
{
    u_int64_t   res;
    u_int32_t   hi = x >> 32;
    u_int32_t   lo = x & 0xffffffff;
    hi = ntohl( hi );
    res = ntohl( lo );
    res = res << 32 | hi;

    return res;
}
#endif


//----------------------------------------------------------------------
Ciphersuite_PI2::Ciphersuite_PI2()
{
}

//----------------------------------------------------------------------
u_int16_t
Ciphersuite_PI2::cs_num(void)
{
    return CSNUM_PI2;
}

//----------------------------------------------------------------------
int
Ciphersuite_PI2::consume(Bundle*    bundle,
                         BlockInfo* block,
                         u_char*    buf,
                         size_t     len)
{
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
Ciphersuite_PI2::validate(const Bundle*           bundle,
                          BlockInfoVec*           block_list,
                          BlockInfo*              block,
                          status_report_reason_t* reception_reason,
                          status_report_reason_t* deletion_reason)
{
    (void)reception_reason;

    size_t          sdnv_len;
    u_char*         buf;
    size_t          len;
    size_t          digest_len;
    size_t          read_digest_len;
    //u_char          ps_digest[EVP_MAX_MD_SIZE];
    u_char*          ps_digest;
    BP_Local_CS*    locals = NULL;
    u_int64_t       field_length;
    std::vector<u_int64_t>              correlator_list;
    std::vector<u_int64_t>::iterator    cl_iter;
    EndpointID      local_eid = BundleDaemon::instance()->local_eid();
    BlockInfoVec::iterator iter;
    u_int16_t       cs_flags;
    int             err = 0;
    DataBuffer      db;
        
    log_debug_p(log, "Ciphersuite_PI2::validate()");
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);
    cs_flags = locals->cs_flags();
    
    if ( destination_is_local_node(bundle, block) )
    {  //yes - this is ours so go to work
            
        if ( !(cs_flags & CS_BLOCK_HAS_RESULT) ) {
            log_err_p(log, "Ciphersuite_PI2::validate: block has no security_result");
            goto fail;
        }
        
        create_digest(bundle, block_list, block, db);   
        digest_len = db.len();

        log_debug_p(log, "Ciphersuite_PI2::validate() digest      0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
        		db.buf()[0], db.buf()[1], db.buf()[2], db.buf()[3], db.buf()[4], db.buf()[5], db.buf()[6], db.buf()[7], db.buf()[8], db.buf()[9], db.buf()[10],
        		db.buf()[11], db.buf()[12], db.buf()[13], db.buf()[14], db.buf()[15], db.buf()[16], db.buf()[17], db.buf()[18], db.buf()[19]);

        // get pieces from results -- should be just the signature
        buf = locals->security_result().buf();
        len = locals->security_result().len();
        
        log_debug_p(log, "Ciphersuite_PI2::validate() security result, len = %zu", len);
        while ( len > 0 ) {
            u_char item_type = *buf++;
            --len;
            sdnv_len = SDNV::decode(buf, len, &field_length);
            buf += sdnv_len;
            len -= sdnv_len;
            
            switch ( item_type ) {
            case CS_signature_field:
            {
                log_debug_p(log, "Ciphersuite_PI2::validate() CS_signature_field item, len %llu", U64FMT(field_length));
                        
                err = KeySteward::verify(bundle, buf, field_length, &ps_digest, &read_digest_len, (int) cs_num());
                CS_FAIL_IF(err != 0);
                log_debug_p(log, "Ciphersuite_PI2::validate() -- KeySteward::verify() returned %d", err);
                locals->set_proc_flag(CS_BLOCK_PASSED_VALIDATION | CS_BLOCK_COMPLETED_DO_NOT_FORWARD);

                if(digest_len != read_digest_len || memcmp(ps_digest, db.buf(), digest_len) != 0) {
                    log_err_p(log, "Ciphersuite_PI2::validate: digests didn't match");
                    goto fail;
                }
                free(ps_digest);
            }
            break;
                    
            default:    // deal with improper items
                log_err_p(log, "Ciphersuite_PI2::validate: unexpected item type %d in security_result", item_type);
                goto fail;
            }
            buf += field_length;
            len -= field_length;
        }
    } else
        locals->set_proc_flag(CS_BLOCK_DID_NOT_FAIL);   // not for here so we didn't check this block
        
    log_debug_p(log, "Ciphersuite_PI2::validate() done");
    
    return true;
    
    
    
 fail:    
    locals->set_proc_flag(CS_BLOCK_FAILED_VALIDATION | CS_BLOCK_COMPLETED_DO_NOT_FORWARD);
    *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
    return false;
}

//----------------------------------------------------------------------
int
Ciphersuite_PI2::prepare(const Bundle*    bundle,
                         BlockInfoVec*    xmit_blocks,
                         const BlockInfo* source,
                         const LinkRef&   link,
                         list_owner_t     list)
{
    (void)bundle;
    (void)link;
    
    int             result = BP_FAIL;
    u_int16_t       cs_flags = 0;
    BP_Local_CS*    locals = NULL;
    BP_Local_CS*    source_locals = NULL;
    EndpointID      local_eid = BundleDaemon::instance()->local_eid();
    BundleDaemon*   bd = BundleDaemon::instance();
    
//XXXpl - fix this test
    if ( (source != NULL)  &&
         (dynamic_cast<BP_Local_CS*>(source->locals())->security_dest() == bd->local_eid().data()) ) {
        log_debug_p(log, "Ciphersuite_PI2::prepare() - not being forwarded");
        return BP_SUCCESS;     //it was for us so don't forward
    }
    
    BlockInfo       bi = BlockInfo(BundleProtocol::find_processor(BundleProtocol::PAYLOAD_SECURITY_BLOCK), source);     // NULL source is OK here
    
    // If this is a received block then there's not a lot to do yet.
    // We copy some parameters - the main work is done in generate().
    // Insertion is at the end of the list, which means that
    // it will be in the same position as received
    if ( list == BlockInfo::LIST_RECEIVED ) {
        
        if ( Ciphersuite::destination_is_local_node(bundle, source) )
            return BP_SUCCESS;     //don't forward if it's for here

        CS_FAIL_IF_NULL(source);
        xmit_blocks->push_back(bi);
        BlockInfo* bp = &(xmit_blocks->back());
        CS_FAIL_IF_NULL(bp);
        bp->set_eid_list(source->eid_list());
        log_debug_p(log, "Ciphersuite_PI2::prepare() - forward received block len %u eid_list_count %zu new count %zu",
                    source->full_length(), source->eid_list().size(), bp->eid_list().size());
        
        CS_FAIL_IF_NULL( source->locals() );      // broken

        source_locals = dynamic_cast<BP_Local_CS*>(source->locals());
        CS_FAIL_IF_NULL(source_locals);           // also broken
        bp->set_locals(new BP_Local_CS);
        locals = dynamic_cast<BP_Local_CS*>(bp->locals());
        CS_FAIL_IF_NULL(locals);
        locals->set_owner_cs_num(CSNUM_PI2);
        cs_flags = source_locals->cs_flags();
        locals->set_list_owner(BlockInfo::LIST_RECEIVED);
        locals->set_correlator(source_locals->correlator());
        bp->writable_contents()->reserve(source->full_length());
        bp->writable_contents()->set_len(0);
        
        // copy security-src and -dest if they exist
        if ( source_locals->cs_flags() & CS_BLOCK_HAS_SOURCE ) {
            CS_FAIL_IF(source_locals->security_src().length() == 0 );
            log_debug_p(log, "Ciphersuite_PI2::prepare() add security_src EID");
            cs_flags |= CS_BLOCK_HAS_SOURCE;
            locals->set_security_src(source_locals->security_src());
        }
        
        if ( source_locals->cs_flags() & CS_BLOCK_HAS_DEST ) {
            CS_FAIL_IF(source_locals->security_dest().length() == 0 );
            log_debug_p(log, "Ciphersuite_PI2::prepare() add security_dest EID");
            cs_flags |= CS_BLOCK_HAS_DEST;
            locals->set_security_dest(source_locals->security_dest());
        }
        locals->set_cs_flags(cs_flags);
        log_debug_p(log, "Ciphersuite_PI2::prepare() - inserted block eid_list_count %zu",
                    bp->eid_list().size());
        result = BP_SUCCESS;
        return result;
    } else {

        // initialize the block
        log_debug_p(log, "Ciphersuite_PI2::prepare() - add new block (or API block etc)");
        bi.set_locals(new BP_Local_CS);
        CS_FAIL_IF_NULL(bi.locals());
        locals = dynamic_cast<BP_Local_CS*>(bi.locals());
        CS_FAIL_IF_NULL(locals);
        locals->set_owner_cs_num(CSNUM_PI2);
        locals->set_list_owner(list);
        
        // if there is a security-src and/or -dest, use it -- might be specified by API
        if ( source != NULL && source->locals() != NULL)  {
            locals->set_security_src(dynamic_cast<BP_Local_CS*>(source->locals())->security_src());
            locals->set_security_dest(dynamic_cast<BP_Local_CS*>(source->locals())->security_dest());
        }
        
        log_debug_p(log, "Ciphersuite_PI2::prepare() local_eid %s bundle->source_ %s", local_eid.c_str(), bundle->source().c_str());
        // if not, and we didn't create the bundle, specify ourselves as sec-src
        if ( (locals->security_src().length() == 0) && (local_eid != bundle->source()))
            locals->set_security_src(local_eid.str());
        
        // if we now have one, add it to list, etc
        if ( locals->security_src().length() > 0 ) {
            log_debug_p(log, "Ciphersuite_PI2::prepare() add security_src EID %s", locals->security_src().c_str());
            cs_flags |= CS_BLOCK_HAS_SOURCE;
            bi.add_eid(locals->security_src());
        }
        
        if ( locals->security_dest().length() > 0 ) {
            log_debug_p(log, "Ciphersuite_PI2::prepare() add security_dest EID %s", locals->security_dest().c_str());
            cs_flags |= CS_BLOCK_HAS_DEST;
            bi.add_eid(locals->security_dest());
        }
            
        locals->set_cs_flags(cs_flags);
            
        // We should already have the primary block in the list.
        // We'll insert this after the primary and any BA blocks
        // and before everything else
        if ( xmit_blocks->size() > 0 ) {
            BlockInfoVec::iterator iter = xmit_blocks->begin();
            
            while ( iter != xmit_blocks->end()) {
                switch (iter->type()) {
                case BundleProtocol::PRIMARY_BLOCK:
                case BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK:
                    ++iter;
                    continue;
                    
                default:
                    break;
                }
                xmit_blocks->insert(iter, bi);
                break;
            }
        } else {
            // it's weird if there are no other blocks but, oh well ...
            xmit_blocks->push_back(bi);
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
int
Ciphersuite_PI2::generate(const Bundle*  bundle,
                          BlockInfoVec*  xmit_blocks,
                          BlockInfo*     block,
                          const LinkRef& link,
                          bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    
    int             result = BP_FAIL;
    size_t          sig_len = 0;
    size_t          res_len = 0;
    size_t          length = 0;
    size_t          param_len = 0;
    u_char          fragment_item[24];             // 24 is enough for 2 max-sized SDNVs and type and length
    u_int16_t       cs_flags = 0;
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    u_char*         ptr;
    size_t          temp;
    size_t          rem;
    DataBuffer      encrypted_key;
    EVP_MD_CTX      ctx;
    size_t          digest_len;
    u_char*         buf = NULL;
    
    int             sdnv_len = 0;       // use an int to handle -1 return values
    int             err = 0;
    int             len = 0;
    BlockInfo::DataBuffer* contents = NULL;
    LocalBuffer*    params = NULL;
        
    log_debug_p(log, "Ciphersuite_PI2::generate() %p", block);
    CS_FAIL_IF_NULL(locals);
    cs_flags = locals->cs_flags();      // get flags from prepare()
    // if this is a received block then it's easy
    if ( locals->list_owner() == BlockInfo::LIST_RECEIVED ) 
    {
        // generate the preamble and copy the data.
        size_t length = block->source()->data_length();
        
        generate_preamble(xmit_blocks, 
                          block,
                          BundleProtocol::PAYLOAD_SECURITY_BLOCK,
                          //BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |  //This causes non-BSP nodes to delete the bundle
                          (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
                          length);

        BlockInfo::DataBuffer* contents = block->writable_contents();
        contents->reserve(block->data_offset() + length);
        contents->set_len(block->data_offset() + length);
        memcpy(contents->buf() + block->data_offset(),
               block->source()->data(), length);
        log_debug_p(log, "Ciphersuite_PI2::generate() %p done", block);
        return BP_SUCCESS;
    }    /**************  forwarding done  **************/
    
    
    /* params field will contain
       - fragment offset and length, if a fragment-bundle, plus type and length
    */

    params = locals->writable_security_params();
    
    param_len = 0;
    
    if ( bundle->is_fragment() ) {
        log_debug_p(log, "Ciphersuite_PI2::generate() bundle is fragment");
        ptr = &fragment_item[2];
        rem = sizeof(fragment_item) - 2;
        temp = SDNV::encode(bundle->frag_offset(), ptr, rem);
        ptr += temp;
        rem -= temp;
        temp += SDNV::encode(bundle->payload().length(), ptr, rem);
        fragment_item[0] = CS_fragment_offset_and_length_field;
        fragment_item[1] = temp;    //guaranteed to fit as a "one-byte SDNV"
        param_len += 2 + temp;
        
    }
    
    if ( param_len > 0 ) {
        cs_flags |= CS_BLOCK_HAS_PARAMS;
        params->reserve(param_len); 
        params->set_len(param_len);
        log_debug_p(log, "Ciphersuite_PI2::generate() security params, len = %zu", param_len);
        
        ptr = params->buf();
        
        if ( bundle->is_fragment() ) 
            memcpy(ptr, fragment_item, 2 + temp);
    }
    
    // need to calculate the size of the security-result items,
    // and the total length of the combined field
    
    /*   result field will contain
         - signed hash, plus type and length
    */
    EVP_MD_CTX_init(&ctx);
    err = EVP_DigestInit_ex(&ctx, EVP_sha256(), NULL);
    CS_FAIL_IF(err == 0);
    digest_len = EVP_MD_CTX_size(&ctx);
    EVP_MD_CTX_cleanup(&ctx);
    
    KeySteward::signature_length(bundle, NULL, link, digest_len, sig_len, (int) cs_num());
    
    res_len = 1 + SDNV::encoding_len(sig_len) + sig_len;
    
    // First we need to work out the lengths and create the preamble
    cs_flags |= CS_BLOCK_HAS_RESULT;
    locals->set_cs_flags(cs_flags);
    length = 0; 
    length += SDNV::encoding_len(CSNUM_PI2);
    length += SDNV::encoding_len(locals->cs_flags());
    
    param_len = locals->security_params().len();
    if(param_len > 0) {
        length += SDNV::encoding_len(param_len) + param_len;
    }
    locals->set_security_result_offset(length);     //remember this for finalize()
    length += SDNV::encoding_len(res_len) + res_len;
        
    contents = block->writable_contents();

    generate_preamble(xmit_blocks, 
                      block,
                      BundleProtocol::PAYLOAD_SECURITY_BLOCK,
                      //BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |  //This causes non-BSP nodes to delete the bundle
                      (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
                      length);
    
    log_debug_p(log, "Ciphersuite_PI2::generate() preamble len %u block len %zu", block->data_offset(), length);
    contents->reserve(block->data_offset() + length);
    contents->set_len(block->data_offset() + length);
    buf = block->writable_contents()->buf() + block->data_offset();
    len = length;
    
    // Assemble data into block contents.
        
    // ciphersuite number and flags
    sdnv_len = SDNV::encode(locals->owner_cs_num(), buf, len);
    CS_FAIL_IF(sdnv_len <= 0);
    buf += sdnv_len;
    len -= sdnv_len;
    
    sdnv_len = SDNV::encode(locals->cs_flags(), buf, len);
    CS_FAIL_IF(sdnv_len <= 0);
    buf += sdnv_len;
    len -= sdnv_len;
            
    if ( param_len > 0 ) {
        log_debug_p(log, "Ciphersuite_PI2::generate: encoding parameters of length %d", param_len);
        // length of params
        sdnv_len = SDNV::encode(param_len, buf, len);
        CS_FAIL_IF(sdnv_len <= 0);
        buf += sdnv_len;
        len -= sdnv_len;
        
        // params data
        memcpy(buf, locals->security_params().buf(), param_len );
        buf += param_len;
        len -= param_len;
    }

    // length of result -- we have to put this in now
    sdnv_len = SDNV::encode(res_len, buf, len);
    if(sdnv_len == 2) {
        log_debug_p(log, "Ciphersuite_PI::generate() encoded res_len=0x%02x%02x", *buf, *(buf+1));
    } else if(sdnv_len == 1) {
        log_debug_p(log, "Ciphersuite_PI::generate() encoded res_len=0x%02x", *buf);
    } else {
        log_debug_p(log, "Ciphersuite_PI::generate() encoded res_len=0x%02x%02x...", *buf, *(buf+1));
    }
    log_debug_p(log, "Ciphersuite_PI::generate() res_len=0x%02x", res_len);
    log_debug_p(log, "Ciphersuite_PI::generate() sdnv_len=%d", sdnv_len);
    buf += sdnv_len;
    len -= sdnv_len;

   
    //  no, no ! Not yet !!    
    //  ASSERT( len == 0 );
    log_debug_p(log, "Ciphersuite_PI2::generate() done");
        

    result = BP_SUCCESS;
    return result;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
Ciphersuite_PI2::finalize(const Bundle*  bundle, 
                          BlockInfoVec*  xmit_blocks,
                          BlockInfo*     block, 
                          const LinkRef& link)
{
    (void)link;
    int             result = BP_FAIL;
    size_t          len;
    size_t          sdnv_len;
    size_t          res_len;
    u_char*         buf;
    BP_Local_CS*    locals = NULL;
    std::vector<u_int64_t>              correlator_list;
    std::vector<u_int64_t>::iterator    cl_iter;
    EndpointID      local_eid = BundleDaemon::instance()->local_eid();
    BlockInfoVec::iterator iter;
    DataBuffer      db_digest;
    DataBuffer      db_signed;
    int             err = 0;
    BlockInfo::DataBuffer* contents = NULL;
    LocalBuffer*    digest_result = NULL;
    size_t          sig_len = 0;
        
    log_debug_p(log, "Ciphersuite_PI2::finalize()");
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);
        
    // if this is a received block then we're done
    if ( locals->list_owner() == BlockInfo::LIST_RECEIVED ) 
        return BP_SUCCESS;
    
    create_digest(bundle, xmit_blocks, block, db_digest);        
    
    err = KeySteward::sign(bundle, NULL, link, db_digest, db_signed, (int) cs_num());
    CS_FAIL_IF(err != 0);
    sig_len = db_signed.len();
    res_len = 1 + SDNV::encoding_len(sig_len) + sig_len;
    
    // build the result item
    digest_result = locals->writable_security_result();
    digest_result->reserve(res_len);
    digest_result->set_len(res_len);
    
    buf = digest_result->buf();
    len = digest_result->len();
    
    *buf++ = Ciphersuite::CS_signature_field;               // item type
    len--;
    
    sdnv_len = SDNV::encode(sig_len, buf, len);
    buf += sdnv_len;
    len -= sdnv_len;
    
    memcpy(buf, db_signed.buf(), sig_len);
    
    
    // now put the result item into the block contents
    contents = block->writable_contents();
    buf = contents->buf();
    len = contents->len();
    buf += block->data_offset();    // we need to add data_offset as well,
    len -= block->data_offset();    // since we're pointing at the whole buffer
    
    buf += locals->security_result_offset();    //and this offset is just within
    len -= locals->security_result_offset();    //the data portion of the buffer
    sdnv_len = SDNV::len(buf);  // size of result-length field
    buf += sdnv_len;            // step over that length field
    len -= sdnv_len;
    memcpy(buf, digest_result->buf(), digest_result->len());
    log_debug_p(log, "Ciphersuite_PI2::finalize() done");
    



    result = BP_SUCCESS;
    return result;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
void
Ciphersuite_PI2::digest(const Bundle*    bundle,
                        const BlockInfo* caller_block,
                        const BlockInfo* target_block,
                        const void*      buf,
                        size_t           len,
                        OpaqueContext*   r)
{
    (void)bundle;
    (void)caller_block;
    (void)target_block;
    log_debug_p(log, "Ciphersuite_PI2::digest() %zu bytes", len);

    EVP_MD_CTX*       pctx = reinterpret_cast<EVP_MD_CTX*>(r);
    
    EVP_DigestUpdate( pctx, buf, len );
}

//----------------------------------------------------------------------
void
Ciphersuite_PI2::create_digest(const Bundle*  bundle, 
                               BlockInfoVec*  block_list,
                               BlockInfo*     block,
                               DataBuffer&    db)
{
    size_t          len;
    size_t          sdnv_len;
    EVP_MD_CTX      ctx;
    OpaqueContext*  r = reinterpret_cast<OpaqueContext*>(&ctx);
    char*           dict;
    u_int32_t       offset;
    u_char*         buf;
    const char*     ptr;
    size_t          plen;
    size_t          digest_len;
    u_char          ps_digest[EVP_MAX_MD_SIZE];
    u_int32_t       rlen = 0;
    u_int32_t       header_len;
    u_char          c;
    u_int64_t       eid_ref_count = 0LLU;
    BP_Local_CS*    locals = NULL;
    BP_Local_CS*    target_locals = NULL;
    u_int64_t       target_flags;
    u_int64_t       flags_save;
    u_int64_t       mask = 0LLU;            /// specify mask for flags
    u_int64_t       mask_primary = 0LLU;    /// mask for primary-block flags
    u_int64_t       target_content_length;
    u_int64_t       correlator;
    u_int64_t       cs_flags;
    u_int64_t       suite_num;
    std::vector<u_int64_t>              correlator_list;
    std::vector<u_int64_t>::iterator    cl_iter;
    EndpointID      local_eid = BundleDaemon::instance()->local_eid();
    BlockInfoVec::iterator iter;
    int             err = 0;
    PrimaryBlock_ex primary;
        
    log_debug_p(log, "Ciphersuite_PI2::create_digest()");
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
        
    // prepare context 
    EVP_MD_CTX_init(&ctx);
    err = EVP_DigestInit_ex(&ctx, EVP_sha256(), NULL);
    digest_len = EVP_MD_CTX_size(&ctx);
    // XXX-pl  check error -- zero is failure
        
    // Walk the list and process each of the blocks.
    // We only digest PS, C3 and the payload data,
    // all others are ignored
    
    // Note that we can only process PSBs and C3s that follow this block
    // as doing otherwise would mean that there would be a
    // correlator block preceding its parent
    
    // There can also be tunnelling issues, depending upon the
    // exact sequencing of blocks. It seems best to add C blocks
    // as early as possible in order to mitigate this problem.
    // That has its own drawbacks unfortunately
    
    header_len =        1       //version
                        +   8       //flags SDNV
                        +   4       //header length itself
                        +   4       //destination eid length
                        +   4       //source eid length
                        +   4       //report-to eid length
                        +   8       //creation SDNV #1
                        +   8       //creation SDNV #2
                        +   8;      //lifetime SDNV
    
    if ( bundle->is_fragment() ) 
        header_len +=   8       //fragment offset SDNV
                        +   8;      //total-length SDNV
    
    // do stuff for primary, and ignore it during the walk
    
    iter = block_list->begin();     //primary
    
    err = read_primary(bundle, &*iter, primary, &dict);
    
    header_len += strlen(dict + primary.dest_scheme_offset);
    header_len += strlen(dict + primary.dest_ssp_offset);
    header_len += strlen(dict + primary.source_scheme_offset);
    header_len += strlen(dict + primary.source_ssp_offset);
    header_len += strlen(dict + primary.replyto_scheme_offset);
    header_len += strlen(dict + primary.replyto_ssp_offset);
    log_debug_p(log, "Ciphersuite_PI2::create_digest() header_len %u", header_len);     


    // Now start the actual digest process
    digest( bundle, block, &*iter, &primary.version, 1, r);     //version
    
    primary.processing_flags &= mask_primary;
    target_flags = htonq(primary.processing_flags);
    digest( bundle, block, &*iter, &primary.processing_flags, sizeof(primary.processing_flags), r);
    
    header_len = htonl(header_len);
    digest( bundle, block, &*iter, &header_len, sizeof(header_len), r);
    
    
    offset = strlen(dict + primary.dest_scheme_offset) + strlen(dict + primary.dest_ssp_offset);    // Note:- "offset" is 4 bytes, not 8
    offset = htonl(offset);
    digest( bundle, block, &*iter, &offset, sizeof(offset), r);
    digest( bundle, block, &*iter, dict + primary.dest_scheme_offset, strlen(dict + primary.dest_scheme_offset), r);
    digest( bundle, block, &*iter, dict + primary.dest_ssp_offset, strlen(dict + primary.dest_ssp_offset), r);

    offset = strlen(dict + primary.source_scheme_offset) + strlen(dict + primary.source_ssp_offset);
    offset = htonl(offset);
    digest( bundle, block, &*iter, &offset, sizeof(offset), r);
    digest( bundle, block, &*iter, dict + primary.source_scheme_offset, strlen(dict + primary.source_scheme_offset), r);
    digest( bundle, block, &*iter, dict + primary.source_ssp_offset, strlen(dict + primary.source_ssp_offset), r);

    offset = strlen(dict + primary.replyto_scheme_offset) + strlen(dict + primary.replyto_ssp_offset);
    offset = htonl(offset);
    digest( bundle, block, &*iter, &offset, sizeof(offset), r);
    digest( bundle, block, &*iter, dict + primary.replyto_scheme_offset, strlen(dict + primary.replyto_scheme_offset), r);
    digest( bundle, block, &*iter, dict + primary.replyto_ssp_offset, strlen(dict + primary.replyto_ssp_offset), r);
    
    // two SDNVs for creation timestamp, one for lifetime
    primary.creation_time = htonq(primary.creation_time);
    digest( bundle, block, &*iter, &primary.creation_time, sizeof(primary.creation_time), r);
    primary.creation_sequence = htonq(primary.creation_sequence);
    digest( bundle, block, &*iter, &primary.creation_sequence, sizeof(primary.creation_sequence), r);
    primary.lifetime = htonq(primary.lifetime);
    digest( bundle, block, &*iter, &primary.lifetime, sizeof(primary.lifetime), r);
    
    if ( bundle->is_fragment() ) {
        primary.fragment_offset = htonq(primary.fragment_offset);
        digest( bundle, block, &*iter, &primary.fragment_offset, sizeof(primary.fragment_offset), r);
        primary.original_length = htonq(primary.original_length);
        digest( bundle, block, &*iter, &primary.original_length, sizeof(primary.original_length), r);
    }
    
    ++iter;     //primary is done now
    
    log_debug_p(log, "Ciphersuite_PI2::create_digest() walk block list");
    for ( ;
          iter != block_list->end();
          ++iter)
    {
        // Advance the iterator to our current position.
        // While we do it, we also remember the correlator values
        // of any PSBs or C3 blocks we encounter.
        // We do this to avoid processing any related correlated blocks
        // Note that we include the current block in the test below
        // in order to prevent encapsulating it !!
        target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
        if ( (&*iter) <= block ) {
            if (  iter->type() == BundleProtocol::PAYLOAD_SECURITY_BLOCK ||
                  (iter->type() == BundleProtocol::CONFIDENTIALITY_BLOCK  &&
                   target_locals->owner_cs_num() == Ciphersuite_PC3::CSNUM_PC3  )  ) {
                if ( target_locals->cs_flags() & CS_BLOCK_HAS_CORRELATOR) {
                    //add correlator to exclude-list
                    correlator_list.push_back(target_locals->correlator());
                }
            }
            continue;
        }
        
        
        switch ( iter->type() ) {
        case BundleProtocol::PAYLOAD_SECURITY_BLOCK:
        case BundleProtocol::CONFIDENTIALITY_BLOCK:
        {
                    
            log_debug_p(log, "Ciphersuite_PI2::create_digest() PS or C block type %d cs_num %d",
                        iter->type(), target_locals->owner_cs_num());
            if (  iter->type() == BundleProtocol::PAYLOAD_SECURITY_BLOCK  &&
                  target_locals->owner_cs_num() != Ciphersuite_PC3::CSNUM_PC3 )  
                continue;       // only digest C3
                    
                    
            // see if there's a correlator and, if there is,
            // if this is a secondary block. Only process a secondary
            // if we also did its primary
            bool    skip_target = false;
            target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
            log_debug_p(log, "Ciphersuite_PI2::create_digest() target_locals->cs_flags 0x%hx", target_locals->cs_flags());
            log_debug_p(log, "Ciphersuite_PI2::create_digest() target_locals->correlator() 0x%llx", U64FMT(target_locals->correlator()));
            if ( target_locals->cs_flags() & CS_BLOCK_HAS_CORRELATOR) {
                correlator = target_locals->correlator();
                for ( cl_iter = correlator_list.begin();
                      cl_iter < correlator_list.end();
                      ++cl_iter) {
                    if ( correlator == *cl_iter) {                              
                        skip_target = true;
                        break;      //break from for-loop
                    }
                }
                if ( skip_target )
                    break;  //break from switch, continue for "for" loop
                        
            }
                    
            log_debug_p(log, "Ciphersuite_PI2::create_digest() digest this block, len %u eid_list().size() %zu", 
                        iter->full_length(), iter->eid_list().size());
            // Either it has no correlator, or it wasn't in the list.
            // So we will process it in the digest
                    
            /**********  start preamble processing  **********/
            buf = iter->contents().buf();
            len = iter->full_length();
                    
                    
            // Process block type
            c = *buf++;
            len--;
            digest( bundle, block, &*iter, &c, 1, r);
                    
            // Process flags
            sdnv_len = SDNV::decode( buf, len, &target_flags);
            buf += sdnv_len;
            len -= sdnv_len;
                    
            flags_save = target_flags;
            target_flags &= mask;
            target_flags = htonq(target_flags);
            digest( bundle, block, &*iter, &target_flags, sizeof(target_flags), r);
                    
            // EID list is next, starting with the count although we don't digest it
            if ( flags_save & BundleProtocol::BLOCK_FLAG_EID_REFS ) {                    
                sdnv_len = SDNV::decode(buf, len, &eid_ref_count);
                buf += sdnv_len;
                len -= sdnv_len;
                log_debug_p(log, "Ciphersuite_PI2::create_digest() eid_ref_count %llu", U64FMT(eid_ref_count));
                                                
                // each ref is a pair of SDNVs, so process 2 * eid_ref_count text pieces
                if ( eid_ref_count > 0 ) {
                    for ( u_int32_t i = 0; i < (2 * eid_ref_count); i++ ) {
                        sdnv_len = SDNV::decode(buf, len, &offset);
                        buf += sdnv_len;
                        len -= sdnv_len;
                                
                        ptr = dict + offset;    //point at item in dictionary
                        plen = strlen(ptr);     // length *without* NULL-terminator
                        digest( bundle, block, &*iter, ptr, plen, r);
                    }
                }       
            }

            // Process data length
            sdnv_len = SDNV::decode(buf, len, &target_content_length);
            buf += sdnv_len;
            len -= sdnv_len;
                    
            target_content_length = htonq(target_content_length);
            digest( bundle, block, &*iter, &target_content_length, sizeof(target_content_length), r);
                    
            // start of data is where to start main digest
            offset = buf - iter->contents().buf();
            ASSERT(offset == iter->data_offset());
            /**********  end of preamble processing  **********/
                    
                    
            /**********  start content processing  **********/
                    
            // if it's the current block, we have to exclude security-result data.
            // Note that security-result-length *is* included
            if ( (&*iter) == block ) {

                // ciphersuite number and flags
                sdnv_len = SDNV::decode(buf,
                                        len,
                                        &suite_num);
                buf += sdnv_len;
                len -= sdnv_len;

                sdnv_len = SDNV::decode(buf,
                                        len,
                                        &cs_flags);
                buf += sdnv_len;
                len -= sdnv_len;
                        
                if ( cs_flags & CS_BLOCK_HAS_RESULT ) {
                    // if there's a security-result we have to ease up to it
                    if ( cs_flags & CS_BLOCK_HAS_CORRELATOR )
                        buf += SDNV::len(buf);      //step over correlator
                            
                    if ( cs_flags & CS_BLOCK_HAS_PARAMS )
                        buf += SDNV::len(buf);      //step over params
                            
                    if ( cs_flags & CS_BLOCK_HAS_RESULT ) {
                        sdnv_len = SDNV::decode(buf, len, &target_content_length);
                        buf += sdnv_len;
                        len -= sdnv_len;
                        buf += SDNV::len(buf);      //step over security-result-length field
                    }
                            
                    len = buf - iter->contents().buf();  //this is the length to use
                }
                // now set buf back to the start of the content
                buf = iter->contents().buf();
            }
                    
            iter->owner()->process( Ciphersuite_PI2::digest,
                                    bundle,
                                    block,
                                    &*iter,
                                    offset,
                                    len,
                                    r);
            /**********  end of content processing  **********/
            log_debug_p(log, "Ciphersuite_PI2::create_digest() digest done %p", &*iter);

        }
        break;  //break from switch, continue for "for" loop
            
        case BundleProtocol::PAYLOAD_BLOCK:
        {
                    
            /**********  start preamble processing  **********/
            buf = iter->contents().buf();
            len = iter->full_length();
                    
                    
            // Process block type
            c = *buf++;
            len--;
            digest( bundle, block, &*iter, &c, 1, r);
                    
            // Process flags
            sdnv_len = SDNV::decode( buf, len, &target_flags);
            buf += sdnv_len;
            len -= sdnv_len;
                                        
            flags_save = target_flags;
            target_flags &= mask;
            target_flags = htonq(target_flags);
            digest( bundle, block, &*iter, &target_flags, sizeof(target_flags), r);
                    
            // EID list is next, starting with the count although we don't digest it
            if ( flags_save & BundleProtocol::BLOCK_FLAG_EID_REFS ) {                    
                sdnv_len = SDNV::decode(buf, len, &eid_ref_count);
                buf += sdnv_len;
                len -= sdnv_len;
                                                
                // each ref is a pair of SDNVs, so process 2 * eid_ref_count text pieces
                if ( eid_ref_count > 0 ) {
                    for ( u_int32_t i = 0; i < (2 * eid_ref_count); i++ ) {
                        sdnv_len = SDNV::decode(buf, len, &offset);
                        buf += sdnv_len;
                        len -= sdnv_len;
                                
                        ptr = dict + offset;    //point at item in dictionary
                        plen = strlen(ptr);     // length *without* NULL-terminator
                        digest( bundle, block, &*iter, ptr, plen, r);
                    }
                }       
            }

            // Process data length
            sdnv_len = SDNV::decode(buf, len, &target_content_length);
            buf += sdnv_len;
            len -= sdnv_len;
                    
            target_content_length = htonq(target_content_length);
            digest( bundle, block, &*iter, &target_content_length, sizeof(target_content_length), r);
                    
            // start of data is where to start main digest
            offset = buf - iter->contents().buf();
            ASSERT(offset == iter->data_offset());
            /**********  end of preamble processing  **********/
                    
            /**********  start content processing  **********/
                                        
            iter->owner()->process( Ciphersuite_PI2::digest,
                                    bundle,
                                    block,
                                    &*iter,
                                    offset,
                                    len,
                                    r);
            /**********  end of content processing  **********/
            log_debug_p(log, "Ciphersuite_PI2::create_digest() PAYLOAD_BLOCK done");
        }
        break;  //break from switch, continue for "for" loop
                
        default:
            continue;
        
        }   // end of switch  
    }       // end of loop-through-all-the-blocks
    
    
    err = EVP_DigestFinal_ex(&ctx, ps_digest, &rlen);
    // XXX-pl  check error -- zero is failure
    
    log_debug_p(log, "Ciphersuite_PI2::create_digest() digest      0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
                ps_digest[0], ps_digest[1], ps_digest[2], ps_digest[3], ps_digest[4], ps_digest[5], ps_digest[6], ps_digest[7], ps_digest[8], ps_digest[9], ps_digest[10],
                ps_digest[11], ps_digest[12], ps_digest[13], ps_digest[14], ps_digest[15], ps_digest[16], ps_digest[17], ps_digest[18], ps_digest[19]);

    EVP_MD_CTX_cleanup(&ctx);
    
    db.reserve(digest_len);
    db.set_len(digest_len);
    memcpy(db.buf(), ps_digest, digest_len);
    
    log_debug_p(log, "Ciphersuite_PI2::create_digest() done");
    
}


//----------------------------------------------------------------------
int
Ciphersuite_PI2::read_primary(const Bundle*    bundle, 
                              BlockInfo*       block,
                              PrimaryBlock_ex& primary,
                              char**           dict)
{
    u_char*         buf;
    size_t          len;

    size_t primary_len = block->full_length();

    buf = block->writable_contents()->buf();
    len = block->writable_contents()->len();

    ASSERT(primary_len == len);

    primary.version = *(u_int8_t*)buf;
    buf += 1;
    len -= 1;
    
    if (primary.version != BundleProtocol::CURRENT_VERSION) {
        log_warn_p(log, "protocol version mismatch %d != %d",
                   primary.version, BundleProtocol::CURRENT_VERSION);
        return -1;
    }
    
#define PBP_READ_SDNV(location) { \
    int sdnv_len = SDNV::decode(buf, len, location); \
    if (sdnv_len < 0) \
        goto tooshort; \
    buf += sdnv_len; \
    len -= sdnv_len; }
    
    // Grab the SDNVs representing the flags and the block length.
    PBP_READ_SDNV(&primary.processing_flags);
    PBP_READ_SDNV(&primary.block_length);

    log_debug_p(log, "parsed primary block: version %d length %u",
                primary.version, block->data_length());    
    
/*
 * it may be that the ASSERT which follows is not appropriate because we're doing this
 * on the outbound side and it seems that data_length() is the same as full_length().
 * But what's remaining should be the same as what is promised.
 log_debug_p(log, "parsed primary block: version %d length %u full_length %u len remaining %zu",
 primary.version, block->data_length(), block->full_length(), len);    
 // What remains in the buffer should now be equal to what the block-length
 // field advertised.
 ASSERT(len == block->data_length());
*/
    ASSERT(len == primary.block_length);
    
    // Read the various SDNVs up to the start of the dictionary.
    PBP_READ_SDNV(&primary.dest_scheme_offset);
    PBP_READ_SDNV(&primary.dest_ssp_offset);
    PBP_READ_SDNV(&primary.source_scheme_offset);
    PBP_READ_SDNV(&primary.source_ssp_offset);
    PBP_READ_SDNV(&primary.replyto_scheme_offset);
    PBP_READ_SDNV(&primary.replyto_ssp_offset);
    PBP_READ_SDNV(&primary.custodian_scheme_offset);
    PBP_READ_SDNV(&primary.custodian_ssp_offset);
    PBP_READ_SDNV(&primary.creation_time);
    PBP_READ_SDNV(&primary.creation_sequence);
    PBP_READ_SDNV(&primary.lifetime);
    PBP_READ_SDNV(&primary.dictionary_length);
    *dict = reinterpret_cast<char*>(buf);
    if (bundle->is_fragment()) {
        PBP_READ_SDNV(&primary.fragment_offset);
        PBP_READ_SDNV(&primary.original_length);
    }
#undef PBP_READ_SDNV
    return 0;
    
 tooshort:
    return -1;
}


} // namespace dtn

#endif /* BSP_ENABLED */

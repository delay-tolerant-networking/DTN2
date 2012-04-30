/*
 *    Copyright 2006-7 SPARTA Inc
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

#include "Ciphersuite_PC3.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "bundling/SDNV.h"
#include "contacts/Link.h"
#include "openssl/rand.h"
#include "gcm/gcm.h"
#include "security/KeySteward.h"

namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
Ciphersuite_PC3::Ciphersuite_PC3()
{
}

//----------------------------------------------------------------------
u_int16_t
Ciphersuite_PC3::cs_num(void)
{
    return CSNUM_PC3;
}

//----------------------------------------------------------------------
int
Ciphersuite_PC3::consume(Bundle* bundle, BlockInfo* block,
                        u_char* buf, size_t len)
{
    log_debug_p(log, "Ciphersuite_PC3::consume()");
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
Ciphersuite_PC3::validate(const Bundle*           bundle,
                         BlockInfoVec*           block_list,
                         BlockInfo*              block,
                         status_report_reason_t* reception_reason,
                         status_report_reason_t* deletion_reason)
{
    (void)reception_reason;


//1. do we have security-dest? If yes, get it, otherwise get bundle-dest
//2. does it match local_eid ??
//3. if not, return true
//4. if it does match, parse and validate the block
//5. the actions must exactly reverse the transforming changes made in finalize()

    Bundle*         deliberate_const_cast_bundle = const_cast<Bundle*>(bundle);
    u_int16_t       cs_flags;
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    EndpointID      local_eid = BundleDaemon::instance()->local_eid();
    size_t          offset;
    size_t          len;
    gcm_ctx_ex      ctx_ex;    // includes OpenSSL context within it
    OpaqueContext*  r = reinterpret_cast<OpaqueContext*>(&ctx_ex);
    bool            changed = false;
    u_char          key[key_len];  //use AES128 16-byte key
    u_char          salt[salt_len];       // salt for GCM
    u_char          iv[iv_len];    // GCM "iv" length is 8 bytes
    u_char          target_iv[iv_len];    // GCM "iv" length is 8 bytes
    u_char          nonce[nonce_len];    // 12 bytes recommended
    u_char          tag[tag_len];    // 128 bits recommended
    u_char          tag_encap[tag_len];    // tag for an encapsulated block
    u_char*         buf;
    u_char*         ptr;
    u_char*         data;
    BP_Local_CS*    target_locals = NULL;
    int             sdnv_len = 0;       // use an int to handle -1 return values
    u_char          item_type;
    int32_t         rem;                // use signed value
    u_int64_t       field_length = 0LL;
    u_int64_t       frag_offset_;   // Offset of fragment in the original bundle
    u_int64_t       orig_length_;   // Length of original bundle
    ret_type        ret = 0;
    DataBuffer      db;
    int 			err = 0;
     
    log_debug_p(log, "Ciphersuite_PC3::validate() %p", block);
    CS_FAIL_IF_NULL(locals);
    cs_flags = locals->cs_flags();
    
    if ( Ciphersuite::destination_is_local_node(bundle, block) )
    {  //yes - this is ours so go to work
    
        // we expect this to be the "first" block, and there might or
        // might not be others. But we should get to this one first and,
        // during the processing, convert any other C3 blocks to their
        // unencapsulated form. That is, when this call is over, there
        // should be no more blocks for us to deal with. Any remaining
        // C3 block should be for a nested instance
        
        // get pieces from params -- salt, iv, range, 
        
        buf = locals->security_params().buf();
        len = locals->security_params().len();
        
        log_debug_p(log, "Ciphersuite_PC3::validate() locals->correlator() 0x%llx", U64FMT(locals->correlator()));
        log_debug_p(log, "Ciphersuite_PC3::validate() security params, len = %zu", len);
        while ( len > 0 ) {
            item_type = *buf++;
            --len;
            sdnv_len = SDNV::decode(buf, len, &field_length);
            buf += sdnv_len;
            len -= sdnv_len;
            
            switch ( item_type ) {
            case CS_IV_field: 
            {
                log_debug_p(log, "Ciphersuite_PC3::validate() iv item, len = %llu", U64FMT(field_length));
                memcpy(iv, buf, iv_len);
                buf += field_length;
                len -= field_length;
            }
            break;
                    
            case CS_PC_block_salt:
            {
                log_debug_p(log, "Ciphersuite_PC3::validate() salt item, len = %llu", U64FMT(field_length));
                memcpy(salt, buf, nonce_len - iv_len);
                buf += field_length;
                len -= field_length;
            }
            break;
                    
            case CS_fragment_offset_and_length_field:
            {
                log_debug_p(log, "Ciphersuite_PC3::validate() frag info item, len = %llu", U64FMT(field_length));
                sdnv_len = SDNV::decode(buf, len, &frag_offset_);
                buf += sdnv_len;
                len -= sdnv_len;
                sdnv_len = SDNV::decode(buf, len, &orig_length_);
                buf += sdnv_len;
                len -= sdnv_len;
            }
            break;
                    
            default:    // deal with improper items
                log_err_p(log, "Ciphersuite_PC3::validate: unexpected item type %d in security_params",
                          item_type);
                goto fail;
            }
        }
        
        // get pieces from results -- key, icv
        buf = locals->security_result().buf();
        len = locals->security_result().len();
        
        log_debug_p(log, "Ciphersuite_PC3::validate() security result, len = %zu", len);
        while ( len > 0 ) {
            item_type = *buf++;
            --len;
            sdnv_len = SDNV::decode(buf, len, &field_length);
            buf += sdnv_len;
            len -= sdnv_len;
            
            switch ( item_type ) {
            case CS_key_info_field: 
            {
            	log_debug_p(log, "Ciphersuite_PC3::validate() key-info item");
            	log_debug_p(log, "Ciphersuite_PC3::validate() key-info (1st 20 bytes) : 0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
            			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
            			buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15],
            			buf[16], buf[17], buf[18], buf[19]);
            	err = KeySteward::decrypt(bundle, locals->security_src(), buf, field_length, db, (int) cs_num());
                CS_FAIL_IF(err != 0);
            	memcpy(key, db.buf(), key_len);
            	log_debug_p(log, "Ciphersuite_PC3::validate() key      0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
            			key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7],
            			key[8], key[9], key[10], key[11], key[12], key[13], key[14], key[15]);

            	buf += field_length;
            	len -= field_length;
            }
            break;

            /*case CS_encoded_key_field:
            {
                log_debug_p(log, "Ciphersuite_PC3::validate() encoded key item");
                KeySteward::decrypt(bundle, locals->security_src(), buf, field_length, db);
                memcpy(key, db.buf(), key_len);
                log_debug_p(log, "Ciphersuite_PC3::validate() key      0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
                            key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7], 
                            key[8], key[9], key[10], key[11], key[12], key[13], key[14], key[15]);
                buf += field_length;
                len -= field_length;
            }
            break;*/


            case CS_PC_block_ICV_field:
            {
                log_debug_p(log, "Ciphersuite_PC3::validate() icv item");
                memcpy(tag, buf, tag_len);
                buf += field_length;
                len -= field_length;
            }
            break;
                    
            case CS_encap_block_field:
            {
                // don't think we should have one of these here,
                // only in the correlated blocks
                log_err_p(log, "Ciphersuite_PC3::validate: unexpected encap block in security_result");
                goto fail;
            }
            break;
                    
            default:    // deal with improper items
                log_err_p(log, "Ciphersuite_PC3::validate: unexpected item type %d in security_result",
                          item_type);
                goto fail;
            }
        }
        
        // prepare context - one time for all usage here
        gcm_init_and_key(key, key_len, &(ctx_ex.c));
        ctx_ex.operation = op_decrypt;

        // we have the necessary pieces from params and result so now
        // walk all the blocks and do the various processing things needed.
        // First is to get the iterator to where we are (see note in "generate()"
        // for why we do this)
        
        log_debug_p(log, "Ciphersuite_PC3::validate() walk block list");
        for (BlockInfoVec::iterator iter = block_list->begin();
             iter != block_list->end();
             ++iter)
        {
            // step over all blocks up to and including the one which
            // prompted this call, pointed at by "block" argument
            if ( (&*iter) <= block )
                continue;
            
            target_locals = dynamic_cast<BP_Local_CS*>(iter->locals()); //might or might not be valid        

            switch ( iter->type() ) {
                
            case BundleProtocol::CONFIDENTIALITY_BLOCK:
            {
                log_debug_p(log, "Ciphersuite_PC3::validate() C block %p", &*iter);
                BlockInfo::DataBuffer    encap_block;
                CS_FAIL_IF_NULL(target_locals);
                // even though this isn't our block, the value will have
                // been set when the block was finished being received
                // (in Ciphersuite::parse)
                log_debug_p(log, "Ciphersuite_PC3::validate() C block owner_cs_num %d", target_locals->owner_cs_num());
                if ( target_locals->owner_cs_num() != CSNUM_PC3 )  
                    continue;        // only decapsulate C3
                      
                // it's a C3 block but make sure we own it -- does the
                // correlator match ??
                if ( target_locals->correlator() != locals->correlator() )
                    continue;        // not ours
                      
                // OK - it's ours and we now decapsulate it.
                // Get the necessary pieces from it, such as iv
                buf = target_locals->security_params().buf();
                len = target_locals->security_params().len();
                    
                log_debug_p(log, "Ciphersuite_PC3::validate() target security params, len = %zu", len);
                while ( len > 0 ) {
                    item_type = *buf++;
                    --len;
                    sdnv_len = SDNV::decode(buf, len, &field_length);
                    buf += sdnv_len;
                    len -= sdnv_len;
                        
                    switch ( item_type ) {
                    case CS_IV_field: 
                    {
                        log_debug_p(log, "Ciphersuite_PC3::validate() target iv item, len = %llu", U64FMT(field_length));
                        memcpy(target_iv, buf, iv_len);
                        buf += field_length;
                        len -= field_length;
                    }
                    break;
                                
                    default:    // deal with improper items
                        log_err_p(log, "Ciphersuite_PC3::validate: unexpected item type %d in target security_params",
                                  item_type);
                        goto fail;
                    }
                }
                    
                buf = target_locals->security_result().buf();
                len = target_locals->security_result().len();
                    
                log_debug_p(log, "Ciphersuite_PC3::validate() target security result, len = %zu", len);
                while ( len > 0 ) {
                    item_type = *buf++;
                    --len;
                    sdnv_len = SDNV::decode(buf, len, &field_length);
                    buf += sdnv_len;
                    len -= sdnv_len;
                        
                    // we don't necessarily know what order these two fields
                    // will be in, so collect both and decrypt afterwards
                    switch ( item_type ) {
                    case CS_PC_block_ICV_field: 
                    {
                        log_debug_p(log, "Ciphersuite_PC3::validate() target icv item, len = %llu", U64FMT(field_length));
                        memcpy(tag_encap, buf, tag_len);
                        buf += field_length;
                        len -= field_length;
                    }
                    break;
                                
                    case CS_encap_block_field: 
                    {
                        log_debug_p(log, "Ciphersuite_PC3::validate() encap block item, len = %llu", U64FMT(field_length));
                        encap_block.reserve(field_length);
                        encap_block.set_len(field_length);
                        memcpy(encap_block.buf(), buf, field_length);
                        buf += field_length;
                        len -= field_length;
                    }
                    break;
                                
                    default:    // deal with improper items
                        log_err_p(log, "Ciphersuite_PC3::validate: unexpected item type %d in target security_result",
                                  item_type);
                        goto fail;
                    }
                }
                    
                // nonce is 12 bytes, first 4 are salt (same for all blocks)
                // and last 8 bytes are per-block IV. The final 4 bytes in
                // the full block-sized field are, of course, the counter
                // which is not represented here
                ptr = nonce;
                    
                memcpy(ptr, salt, nonce_len - iv_len);
                ptr += nonce_len - iv_len;
                memcpy(ptr, target_iv, iv_len);
                    
                // prepare context
                gcm_init_message(nonce, nonce_len, &(ctx_ex.c));
                    
                // decrypt message
                ret = gcm_decrypt_message(nonce, 
                                          nonce_len, 
                                          NULL, 
                                          0, 
                                          encap_block.buf(),
                                          encap_block.len(),
                                          tag_encap,                // tag is input, for validation against calculated tag
                                          tag_len,
                                          &(ctx_ex.c));
                    
                // check return value that the block was OK
                if ( ret != 0 ) {
                    log_err_p(log, "Ciphersuite_PC3::validate: gcm_decrypt_message failed, ret = %d", ret);
                    goto fail;
                }
                    
                // encap_block is the raw data of the encapsulated block
                // and now we have to reconstitute it the way it used to be :)
                    
                // Parse the content as would be done for a newly-received block
                // using the owner's consume() method 
                    
                // We need to stitch up the EID lists as the list in the block is broken. 
                // The way to do this is to create a slightly-synthetic preamble
                // with the appropriate eid-offsets in it. The pre-existing list has been
                // preserved and carried along. But the offsets contained in the preamble
                // refer to an outdated image of the dictionary. So we copy the offsets
                // from the *current* block into the synthetic preamble.
                // The list will then have the correct pointers into the dictionary, 
                // as those will have been updated at all the intermediate nodes.
                // The remainder of the preamble comes from the encapsulated block. 
                    
                data = encap_block.buf();
                len = encap_block.len();
                    
                BlockInfo info(BundleProtocol::find_processor(*data));
                u_int64_t eid_ref_count = 0LLU;
                u_int64_t current_eid_count;
                u_int64_t flags;
                u_int64_t content_length = 0LLU;
                    
                BlockInfo::DataBuffer    preamble;
                preamble.reserve(iter->full_length());    //can't be bigger
                // do set_len() later
                    
                // copy bits and pieces from the decrypted block
                ptr = preamble.buf();
                rem = iter->full_length();
                    
                *ptr++ = *data++;                // block type
                rem--;
                len--;
                sdnv_len = SDNV::decode(data, len, &flags);        // block processing flags (SDNV)
                data += sdnv_len;
                len -= sdnv_len;
                log_debug_p(log, "Ciphersuite_PC3::validate() target block type %hhu flags 0x%llx", *(preamble.buf()), U64FMT(flags));
                // Also see if there are EID refs, and if there will be any in 
                // the resultant block
                    
                // EID list is next, starting with the count
                if  ( flags & BundleProtocol::BLOCK_FLAG_EID_REFS ) {                    
                    sdnv_len = SDNV::decode(data, len, &eid_ref_count);
                    data += sdnv_len;
                    len -= sdnv_len;
                        
                    current_eid_count = iter->eid_list().size();
                        
                    if ( eid_ref_count != current_eid_count ) {
                        log_err_p(log, "Ciphersuite_PC3::validate: eid_ref_count %lld  != current_eid_count %lld", 
                                  U64FMT(eid_ref_count), U64FMT(current_eid_count));
                        goto fail;        // block is broken somehow
                    }
                }

                // each ref is a pair of SDNVs, so step over 2 * eid_ref_count
                if ( eid_ref_count > 0 ) {
                    for ( u_int32_t i = 0; i < (2 * eid_ref_count); i++ ) {
                        sdnv_len = SDNV::len(data);
                        data += sdnv_len;
                        len -= sdnv_len;
                    }
                }        // now we're positioned after the broken refs, if any
                sdnv_len = SDNV::decode(data, len, &content_length);
                data += sdnv_len;
                len -= sdnv_len;
                log_debug_p(log, "Ciphersuite_PC3::validate() target data content size %llu", U64FMT(content_length));

                // fix up last-block flag
                // this probably isn't the last block, but who knows ? :)
                if ( iter->flags() & BundleProtocol::BLOCK_FLAG_LAST_BLOCK ) 
                    flags |= BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
                else
                    flags &= ~BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
                    
                // put flags into the adjusted block
                sdnv_len = SDNV::encode(flags, ptr, rem);
                ptr += sdnv_len;
                rem -= sdnv_len;
                    
                // copy the offsets from the current block
                if ( eid_ref_count > 0 ) {
                    u_char*        cur_ptr = iter->contents().buf();
                    size_t        cur_len = iter->full_length();
                        
                    cur_ptr++;    //type field
                    cur_len--;
                    sdnv_len = SDNV::len(cur_ptr);    //flags
                    cur_ptr += sdnv_len;
                    cur_len -= sdnv_len;
                        
                    sdnv_len = SDNV::len(cur_ptr);    //eid ref count
                    cur_ptr += sdnv_len;
                    cur_len -= sdnv_len;
                        
                    // put eid_count into the adjusted block
                    log_debug_p(log, "Ciphersuite_PC3::validate() eid_ref_count %lld", U64FMT(eid_ref_count));
                    sdnv_len = SDNV::encode(eid_ref_count, ptr, rem);
                    ptr += sdnv_len;
                    rem -= sdnv_len;
                        
                    // now copy the reference pairs
                    for ( u_int32_t i = 0; i < (2 * eid_ref_count); i++ ) {
                        sdnv_len = SDNV::len(cur_ptr);
                        memcpy(ptr, cur_ptr, sdnv_len);
                        cur_ptr += sdnv_len;
                        cur_len -= sdnv_len;
                        ptr += sdnv_len;
                        rem -= sdnv_len;
                    }
                }
                    
                // length of data content in block
                sdnv_len = SDNV::encode(content_length, ptr, rem);
                ptr += sdnv_len;
                rem -= sdnv_len;
                    
                // we now have a preamble in "preamble" and the rest of the data at *data
                size_t    preamble_size = ptr - preamble.buf();
                preamble.set_len(preamble_size);
                log_debug_p(log, "Ciphersuite_PC3::validate() target preamble_size %zu", preamble_size);
                    
                     
                {
                    // we're reusing the existing BlockInfo but we need to clean it first
                    iter->~BlockInfo();
                    /* we'd like to reinitilize the block thusly
                     *      iter->BlockInfo(type);
                     * but C++ gets bent so we have to achieve the desired result
                     * in a more devious fashion using placement-new. 
                     */
                        
                    log_debug_p(log, "Ciphersuite_PC3::validate() re-init target");
                    BlockInfo* bp = &*iter;
                    bp = new (bp) BlockInfo(BundleProtocol::find_processor(*(preamble.buf())));
                    CS_FAIL_IF_NULL(bp);
                }
                    
                // process preamble
                log_debug_p(log, "Ciphersuite_PC3::validate() process target preamble");
                int cc = iter->owner()->consume(deliberate_const_cast_bundle, &*iter, preamble.buf(), preamble_size);
                if (cc < 0) {
                    log_err_p(log, "Ciphersuite_PC3::validate: consume failed handling encapsulated preamble 0x%x, cc = %d",
                              info.type(), cc);
                    goto fail;
                }
                    
                // process the main part of the encapsulated block
                log_debug_p(log, "Ciphersuite_PC3::validate() process target content");
                cc = iter->owner()->consume(deliberate_const_cast_bundle, &*iter, data, len);
                if (cc < 0) {
                    log_err_p(log, "Ciphersuite_PC3::validate: consume failed handling encapsulated block 0x%x, cc = %d",
                              info.type(), cc);
                    goto fail;
                }
                log_debug_p(log, "Ciphersuite_PC3::validate() decapsulation done");
            }
            break;
                    
            case BundleProtocol::PAYLOAD_BLOCK: 
            {
                log_debug_p(log, "Ciphersuite_PC3::validate() PAYLOAD_BLOCK");
                u_char          tag_calc[tag_len];
                // nonce is 12 bytes, first 4 are salt (same for all blocks)
                // and last 8 bytes are per-block IV. The final 4 bytes in
                // the full block-sized field are, of course, the counter
                // which is not represented here
                ptr = nonce;
                    
                memcpy(ptr, salt, salt_len);
                ptr += salt_len;
                memcpy(ptr, iv, iv_len);
                log_debug_p(log, "Ciphersuite_PC3::validate() nonce    0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
                            nonce[0], nonce[1], nonce[2], nonce[3], nonce[4], nonce[5], nonce[6], nonce[7], nonce[8], nonce[9], nonce[10], nonce[11]);

                // prepare context
                gcm_init_message(nonce, nonce_len, &(ctx_ex.c));
                    
                offset = iter->data_offset();
                len = iter->data_length();

                changed =
                    iter->owner()->mutate( Ciphersuite_PC3::do_crypt,
                                           deliberate_const_cast_bundle,
                                           block,
                                           &*iter,
                                           offset,
                                           len,
                                           r );
                
                // collect the tag (icv) from the context
                gcm_compute_tag( tag_calc, tag_len, &(ctx_ex.c) );
                log_debug_p(log, "Ciphersuite_PC3::validate() tag      0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
                            tag[0], tag[1], tag[2], tag[3], tag[4], tag[5], tag[6], tag[7], tag[8], tag[9], tag[10], tag[11], tag[12], tag[13], tag[14], tag[15]);
                log_debug_p(log, "Ciphersuite_PC3::validate() tag_calc 0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
                            tag_calc[0], tag_calc[1], tag_calc[2], tag_calc[3], tag_calc[4], tag_calc[5], tag_calc[6], tag_calc[7], 
                            tag_calc[8], tag_calc[9], tag_calc[10], tag_calc[11], tag_calc[12], tag_calc[13], tag_calc[14], tag_calc[15]);

                if (memcmp(tag, tag_calc, tag_len) != 0) {
                    log_err_p(log, "Ciphersuite_PC3::validate: tag comparison failed");
                    goto fail;
                }
                    
            }
            break;
                    
            default: 
                continue;
                    
            }    // end switch
        }        // end for
        log_debug_p(log, "Ciphersuite_PC3::validate() walk block list done");
        locals->set_proc_flag(CS_BLOCK_PASSED_VALIDATION |
                              CS_BLOCK_COMPLETED_DO_NOT_FORWARD);
    } else
        locals->set_proc_flag(CS_BLOCK_DID_NOT_FAIL);   // not for here so we didn't check this block

    log_debug_p(log, "Ciphersuite_PC3::validate() %p done", block);
    return true;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(CS_BLOCK_FAILED_VALIDATION |
                              CS_BLOCK_COMPLETED_DO_NOT_FORWARD);
    *deletion_reason = BundleProtocol::REASON_SECURITY_FAILED;
    return false;

}

//----------------------------------------------------------------------
int
Ciphersuite_PC3::prepare(const Bundle*    bundle,
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
        log_debug_p(log, "Ciphersuite_PC3::prepare() - not being forwarded");
        return BP_SUCCESS;     //it was for us so don't forward
    }
    
    BlockInfo bi = BlockInfo(BundleProtocol::find_processor(BundleProtocol::CONFIDENTIALITY_BLOCK), source);        // NULL source is OK here
    
    // If this is a received block then there's not a lot to do yet.
    // We copy some parameters - the main work is done in generate().
    // Insertion is at the end of the list, which means that
    // it will be in the same position as received
    if ( list == BlockInfo::LIST_RECEIVED ) {
        
        ASSERT(source != NULL);
        if ( Ciphersuite::destination_is_local_node(bundle, source) )
            return BP_SUCCESS;     //don't forward if it's for here

        xmit_blocks->push_back(bi);
        BlockInfo* bp = &(xmit_blocks->back());
        bp->set_eid_list(source->eid_list());
        log_debug_p(log, "Ciphersuite_PC3::prepare() - forward received block len %u eid_list_count %zu new count %zu",
                    source->full_length(), source->eid_list().size(), bp->eid_list().size());
        
        CS_FAIL_IF_NULL( source->locals() )       // broken

            source_locals = dynamic_cast<BP_Local_CS*>(source->locals());
        CS_FAIL_IF_NULL(source_locals);
        bp->set_locals(new BP_Local_CS);
        locals = dynamic_cast<BP_Local_CS*>(bp->locals());
        CS_FAIL_IF_NULL(locals);
        locals->set_owner_cs_num(CSNUM_PC3);
        cs_flags = source_locals->cs_flags();
        locals->set_list_owner(BlockInfo::LIST_RECEIVED);
        locals->set_correlator(source_locals->correlator());
        bp->writable_contents()->reserve(source->full_length());
        bp->writable_contents()->set_len(0);
        
        // copy security-src and -dest if they exist
        if ( source_locals->cs_flags() & CS_BLOCK_HAS_SOURCE ) {
            CS_FAIL_IF(source_locals->security_src().length() == 0 );
            log_debug_p(log, "Ciphersuite_PC3::prepare() add security_src EID");
            cs_flags |= CS_BLOCK_HAS_SOURCE;
            locals->set_security_src(source_locals->security_src());
        }
        
        if ( source_locals->cs_flags() & CS_BLOCK_HAS_DEST ) {
            CS_FAIL_IF(source_locals->security_dest().length() == 0 );
            log_debug_p(log, "Ciphersuite_PC3::prepare() add security_dest EID");
            cs_flags |= CS_BLOCK_HAS_DEST;
            locals->set_security_dest(source_locals->security_dest());
        }
        locals->set_cs_flags(cs_flags);
        log_debug_p(log, "Ciphersuite_PC3::prepare() - inserted block eid_list_count %zu",
                    bp->eid_list().size());
        result = BP_SUCCESS;
        return result;
    } else {

        // initialize the block
        log_debug_p(log, "Ciphersuite_PC3::prepare() - add new block (or API block etc)");
        bi.set_locals(new BP_Local_CS);
        CS_FAIL_IF_NULL(bi.locals());
        locals = dynamic_cast<BP_Local_CS*>(bi.locals());
        CS_FAIL_IF_NULL(locals);
        locals->set_owner_cs_num(CSNUM_PC3);
        locals->set_list_owner(list);
        
        // if there is a security-src and/or -dest, use it -- might be specified by API
        if ( source != NULL && source->locals() != NULL)  {
            locals->set_security_src(dynamic_cast<BP_Local_CS*>(source->locals())->security_src());
            locals->set_security_dest(dynamic_cast<BP_Local_CS*>(source->locals())->security_dest());
        }
        
        log_debug_p(log, "Ciphersuite_PC3::prepare() local_eid %s bundle->source_ %s", local_eid.c_str(), bundle->source().c_str());
        // if not, and we didn't create the bundle, specify ourselves as sec-src
        if ( (locals->security_src().length() == 0) && (local_eid != bundle->source()))
            locals->set_security_src(local_eid.str());
        
        // if we now have one, add it to list, etc
        if ( locals->security_src().length() > 0 ) {
            log_debug_p(log, "Ciphersuite_PC3::prepare() add security_src EID %s", locals->security_src().c_str());
            cs_flags |= CS_BLOCK_HAS_SOURCE;
            bi.add_eid(locals->security_src());
        }
        
        if ( locals->security_dest().length() > 0 ) {
            log_debug_p(log, "Ciphersuite_PC3::prepare() add security_dest EID %s", locals->security_dest().c_str());
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
Ciphersuite_PC3::generate(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link,
                         bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    
    int             result = BP_FAIL;
    u_char          key[key_len];  //use AES128 16-byte key
    u_char          iv[iv_len];    // AES iv length
    u_char          salt[nonce_len - iv_len];       // salt for GCM
    u_char          fragment_item[24];               // 24 is enough for 2 max-sized SDNVs and type and length
    u_int16_t       cs_flags = 0;
    bool            need_correlator = false;
    u_int64_t       correlator = 0LLU;  
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    BP_Local_CS*    target_locals = NULL;
    u_char*         ptr;
    size_t          temp;
    size_t          rem;
    DataBuffer      encrypted_key;
    size_t          param_len = 0;
    size_t          res_len = 0;
    size_t          length = 0;
    u_char*         buf = NULL;
    int             len = 0;
    int             sdnv_len = 0;       // use an int to handle -1 return values
    u_int16_t       n = 0;
    int             err = 0;
    BlockInfo::DataBuffer* contents = NULL;
    LocalBuffer* digest_result = NULL;
    LocalBuffer* params = NULL;
    
    log_debug_p(log, "Ciphersuite_PC3::generate() %p", block);
    
    CS_FAIL_IF_NULL(locals);
    cs_flags = locals->cs_flags();        // get flags from prepare()
    // if this is a received block then it's easy
    if ( locals->list_owner() == BlockInfo::LIST_RECEIVED ) 
    {
        // generate the preamble and copy the data.
        size_t length = block->source()->data_length();
        
        generate_preamble(xmit_blocks, 
                          block,
                          BundleProtocol::CONFIDENTIALITY_BLOCK,
                          //BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |  //This causes non-BSP nodes to delete the bundle
                          BundleProtocol::BLOCK_FLAG_REPLICATE           |
                          (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
                          length);

        BlockInfo::DataBuffer* contents = block->writable_contents();
        contents->reserve(block->data_offset() + length);
        contents->set_len(block->data_offset() + length);
        memcpy(contents->buf() + block->data_offset(),
               block->source()->data(), length);
        log_debug_p(log, "Ciphersuite_PC3::generate() %p done", block);
        return BP_SUCCESS;
    }
    
    // This block will have a correlator iff there are PSBs or CBs,
    // no correlator if only a payload and no PSBs or CBs
    for (BlockInfoVec::iterator iter = xmit_blocks->begin();
         iter != xmit_blocks->end();
         ++iter)
    {
        n++;
        // Advance the iterator to our current position.
        // Long-winded implementation note:-
        // we would use "distance" but block isn't
        // an iterator, just a pointer. Pointer arithmetic
        // works in some systems but is not always portable
        // so we don't do that here.
        if ( (&*iter) <= block )
            continue;
        
        if (  iter->type() == BundleProtocol::PAYLOAD_SECURITY_BLOCK ) {
            need_correlator = true;     // yes - we need a correlator
            break;
        }
        
        if (  iter->type() == BundleProtocol::CONFIDENTIALITY_BLOCK ) {
            target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
            CS_FAIL_IF_NULL(target_locals);    
            if ( target_locals->owner_cs_num() == CSNUM_PC3 ) {
                need_correlator = true;     // yes - we need a correlator
                break;
            }
        }
        
    }
    
    if ( need_correlator ) {
        correlator = create_correlator(bundle, xmit_blocks);
        correlator |= (int)CSNUM_PC3 << 16;      // add our ciphersuite number
        locals->set_correlator( correlator );
        log_debug_p(log, "Ciphersuite_PC3::generate() correlator %llu", U64FMT(correlator));
    }
        
    /* params field will contain
       - salt (4 bytes), plus type and length
       - IV (block-length, 8 bytes), plus type and length
       - fragment offset and length, if a fragment-bundle, plus type and length
       - key-identifier (optional, not implemented yet), plus type and length
    */

    params = locals->writable_security_params();
    
    param_len = 1 + 1 + sizeof(salt);        // salt
    param_len += 1 + 1 + sizeof(iv);            // IV
 
    if(!bundle->payload_bek_set()) {
        // populate salt and IV
        RAND_bytes(salt, sizeof(salt));
        RAND_bytes(iv, sizeof(iv));
        // generate actual key
        RAND_bytes(key, sizeof(key));
        const_cast<Bundle*>(bundle)->set_payload_bek(key, sizeof(key), iv, salt);
    } else {
        memcpy(key, bundle->payload_bek(), sizeof(key));
        memcpy(salt, bundle->payload_salt(), sizeof(salt));
        memcpy(iv, bundle->payload_iv(), sizeof(iv));
    }

    
    // save for finalize()
    locals->set_key(key, sizeof(key));
    locals->set_salt(salt, sizeof(salt));
    locals->set_iv(iv, sizeof(iv));

   
    if ( bundle->is_fragment() ) {
        log_debug_p(log, "Ciphersuite_PC3::generate() bundle is fragment");
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
    
    params->reserve(param_len);    //will need more if there is a key identifier - TBD
    params->set_len(param_len);
    log_debug_p(log, "Ciphersuite_PC3::generate() security params, len = %zu", param_len);
    
    ptr = params->buf();
    *ptr++ = CS_PC_block_salt;
    *ptr++ = sizeof(salt);                // less than 127
    memcpy(ptr, salt, sizeof(salt));
    ptr += sizeof(salt);
    *ptr++ = CS_IV_field;
    *ptr++ = sizeof(iv);                // less than 127
    memcpy(ptr, iv, sizeof(iv));
    ptr += sizeof(iv);
    
    if ( bundle->is_fragment() ) 
        memcpy(ptr, fragment_item, 2 + temp);
    
    
    // need to calculate the size of the security-result items,
    // and the total length of the combined field
    
    /*   result field will contain
         - encrypted key, plus type and length
         - ICV (Integrity Check Value), plus type and length
    */
    
    /* encrypt the key, keeping a local copy --
       put it directly into the result field
    */


    log_debug_p(log, "Ciphersuite_PC3::generate() key      0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
                key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7], 
                key[8], key[9], key[10], key[11], key[12], key[13], key[14], key[15]);
    err = KeySteward::encrypt(bundle, NULL, link, locals->security_dest(), key, sizeof(key), encrypted_key, (int) cs_num());
    CS_FAIL_IF(err != 0);
    log_debug_p(log, "Ciphersuite_PC3::generate() encrypted_key len = %zu", encrypted_key.len());
    
    res_len = 1 + SDNV::encoding_len(encrypted_key.len()) + encrypted_key.len();
    res_len += 1 + 1 + tag_len;
    
    digest_result = locals->writable_security_result();
    digest_result->reserve(res_len);
    digest_result->set_len(res_len);
    rem = res_len;
    
    ptr = digest_result->buf();
    *ptr++ = Ciphersuite::CS_key_info_field;
    rem--;
    temp = SDNV::encode(encrypted_key.len(), ptr, rem);
    ptr += temp;
    rem -= temp;
    memcpy(ptr, encrypted_key.buf(), encrypted_key.len());
    ptr += encrypted_key.len();
    rem -= encrypted_key.len();
    
    // First we need to work out the lengths and create the preamble
    length = 0;       
    if ( need_correlator ) {
        log_debug_p(log, "Ciphersuite_PC3::generate() correlator %llu", U64FMT(correlator));
        locals->set_correlator(correlator);
        length += SDNV::encoding_len(locals->correlator());
        cs_flags |= CS_BLOCK_HAS_CORRELATOR;
    }
    
    // ciphersuite number and flags
    cs_flags |= CS_BLOCK_HAS_PARAMS;
    cs_flags |= CS_BLOCK_HAS_RESULT;
    locals->set_cs_flags(cs_flags);
    length += SDNV::encoding_len(CSNUM_PC3);
    length += SDNV::encoding_len(locals->cs_flags());
    
    param_len = locals->security_params().len();
    length += SDNV::encoding_len(param_len) + param_len;
    locals->set_security_result_offset(length);        //remember this for finalize()
    length += SDNV::encoding_len(res_len) + res_len;
        
    contents = block->writable_contents();

    generate_preamble(xmit_blocks, 
                      block,
                      BundleProtocol::CONFIDENTIALITY_BLOCK,
                      //BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |  //This causes non-BSP nodes to delete the bundle
                      (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
                      length);
    

    log_debug_p(log, "Ciphersuite_PC3::generate() preamble len %u block len %zu", block->data_offset(), length);
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
            
    if ( need_correlator ) {
        // correlator
        sdnv_len = SDNV::encode(locals->correlator(), buf, len);
        CS_FAIL_IF(sdnv_len <= 0);
        buf += sdnv_len;
        len -= sdnv_len;
    }
    
    
    // length of params
    sdnv_len = SDNV::encode(param_len, buf, len);
    CS_FAIL_IF(sdnv_len <= 0);
    buf += sdnv_len;
    len -= sdnv_len;
    
    // params data
    memcpy(buf, locals->security_params().buf(), param_len );
    buf += param_len;
    len -= param_len;

    // length of result -- we have to put this in now
    sdnv_len = SDNV::encode(res_len, buf, len);

    
    //  no, no ! Not yet !!    
    //  ASSERT( len == 0 );
    log_debug_p(log, "Ciphersuite_PC3::generate() done");
        

    result = BP_SUCCESS;
    return result;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
Ciphersuite_PC3::finalize(const Bundle*  bundle, 
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block, 
                         const LinkRef& link)
{
    (void)link;
    int             result = BP_FAIL;
    Bundle*         deliberate_const_cast_bundle = const_cast<Bundle*>(bundle);
    size_t          offset;
    size_t          len;
    size_t          length;
    size_t          param_len;
    size_t          res_len;
    gcm_ctx_ex      ctx_ex;    // includes OpenSSL context within it
    OpaqueContext*  r = reinterpret_cast<OpaqueContext*>(&ctx_ex);
    bool            changed = false;
    u_char          key[key_len];  //use AES128 16-byte key
    u_char          iv[iv_len];    // GCM "iv" length is 8 bytes
    u_char          nonce[nonce_len];    // 12 bytes recommended
    u_char          tag[tag_len];    // 128 bits recommended
    u_char*         buf;
    u_char*         ptr;
    BP_Local_CS*    locals = NULL;
    BP_Local_CS*    target_locals = NULL;
    u_int64_t       correlator;
    std::vector<u_int64_t>           correlator_list;
    std::vector<u_int64_t>::iterator cl_iter;
    size_t          correlator_size = 0;
    int             sdnv_len = 0;       // use an int to handle -1 return values
    EndpointID      local_eid = BundleDaemon::instance()->local_eid();
        
    log_debug_p(log, "Ciphersuite_PC3::finalize()");
    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    CS_FAIL_IF_NULL(locals);
        
    // if this is a received block then we're done
    if ( locals->list_owner() == BlockInfo::LIST_RECEIVED ) 
        return BP_SUCCESS;
    
    // prepare context - one time for all usage here
    memcpy(key, locals->key().buf(), key_len);
    gcm_init_and_key(key, key_len, &(ctx_ex.c));
    ctx_ex.operation = op_encrypt;
        
    // Walk the list and process each of the blocks.
    // We only change PS, C3 and the payload data,
    // all others are unmodified
    
    // Note that we can only process PSBs and C3s that follow this block
    // as doing otherwise would mean that there would be a
    // correlator block preceding its parent
    
    // However this causes a problem if the PS is a two-block scheme,
    // as we'll convert the second, correlated block to C and then
    // the PS processor won't have its second block.
    
    // There can also be tunnelling issues, depending upon the
    // exact sequencing of blocks. It seems best to add C blocks
    // as early as possible in order to mitigate this problem.
    // That has its own drawbacks unfortunately
    
    
    log_debug_p(log, "Ciphersuite_PC3::finalize() walk block list");
    for (BlockInfoVec::iterator iter = xmit_blocks->begin();
         iter != xmit_blocks->end();
         ++iter)
    {
        // Advance the iterator to our current position.
        // While we do it, we also remember the correlator values
        // of any PSBs or C3 blocks we encounter.
        // We do this to avoid processing any related correlated blocks
        // Note that we include the current block in the test below
        // in order to prevent encapsulating it !!
        if ( (&*iter) <= block ) {
            if ( iter->type() == BundleProtocol::PAYLOAD_SECURITY_BLOCK ) {
                //add correlator to exclude-list
                target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
                CS_FAIL_IF_NULL(target_locals);
                correlator_list.push_back(target_locals->correlator());
            } else if (iter->type() == BundleProtocol::CONFIDENTIALITY_BLOCK ) {
                target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
                CS_FAIL_IF_NULL(target_locals);
                if ( target_locals->owner_cs_num() == CSNUM_PC3 ) {
                    correlator_list.push_back(target_locals->correlator());
                }
            }
            continue;
        }
        
        
        switch ( iter->type() ) {
            
        case BundleProtocol::PAYLOAD_SECURITY_BLOCK:
        case BundleProtocol::CONFIDENTIALITY_BLOCK:
        {
                    
            target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
            CS_FAIL_IF_NULL(target_locals);
            log_debug_p(log, "Ciphersuite_PC3::finalize() PS or C block type %d cs_num %d",
                        iter->type(), target_locals->owner_cs_num());
            if (  iter->type() == BundleProtocol::CONFIDENTIALITY_BLOCK  &&
                  target_locals->owner_cs_num() != CSNUM_PC3                )  
                continue;        // only encapsulate C3
                    
                    
            // see if there's a correlator and, if there is,
            // if this is a secondary block. Only process a secondary
            // if we also did the primary
            bool    skip_psb = false;
            target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
            log_debug_p(log, "Ciphersuite_PC3::finalize() target_locals->cs_flags 0x%hx", target_locals->cs_flags());
            log_debug_p(log, "Ciphersuite_PC3::finalize() target_locals->correlator() 0x%llx", U64FMT(target_locals->correlator()));
            if ( target_locals->cs_flags() & CS_BLOCK_HAS_CORRELATOR) {
                correlator = target_locals->correlator();
                for ( cl_iter = correlator_list.begin();
                      cl_iter < correlator_list.end();
                      ++cl_iter) {
                    if ( correlator == *cl_iter) {                                
                        skip_psb = true;
                        break;        //break from for-loop
                    }
                }
                if ( skip_psb )
                    break;  //break from switch, continue for "for" loop
                        
            }
                    
            log_debug_p(log, "Ciphersuite_PC3::finalize() encapsulate this block, len %u eid_ref_count %zu", 
                        iter->full_length(), iter->eid_list().size());
            // Either it has no correlator, or it wasn't in the list.
            // So we will encapsulate it into a C block. 
            // We need to get the entire content and encrypt it, 
            // then release the locals since we are changing ownership/type.
            // First thing to do is encrypt the entire target block
                    
            // extract the last-block flag since we'll need it shortly
            bool    last = iter->flags() & BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
                    
            // nonce is 12 bytes, first 4 are salt (same for all blocks)
            // and last 8 bytes are per-block IV. The final 4 bytes in
            // the full block-sized field are, of course, the counter
            // which is not represented here
            ptr = nonce;
                    
            memcpy(ptr, locals->salt().buf(), nonce_len - iv_len);
            ptr += nonce_len - iv_len;
            RAND_bytes(iv, sizeof(iv));    // populate IV
            memcpy(ptr, iv, iv_len);
                    
            // prepare context
            gcm_init_message(nonce, nonce_len, &(ctx_ex.c));
                    
            // encrypt message in-place
            gcm_encrypt_message(nonce, 
                                nonce_len, 
                                NULL, 
                                0, 
                                iter->writable_contents()->buf(),
                                iter->full_length(),
                                tag,
                                tag_len,
                                &(ctx_ex.c));
                    
            // copy encrypted block before it gets overwritten
            BlockInfo::DataBuffer    encap_block;
            size_t  encap_len = 1 + SDNV::encoding_len(iter->full_length()) + iter->full_length();
            encap_block.reserve(encap_len);
            encap_block.set_len(encap_len);
            ptr = encap_block.buf();
            *ptr++ = CS_encap_block_field;
            sdnv_len = SDNV::encode(iter->full_length(), ptr, encap_len - 1);
            CS_FAIL_IF(sdnv_len <= 0);                    
            ptr += sdnv_len;
            memcpy(ptr, iter->contents().buf(), iter->full_length());
                    
            // copy C3 locals to new locals block, but don't
            // replace old locals block yet
            BP_Local_CS* new_target_locals = new BP_Local_CS(*locals);
            u_int16_t cs_flags = CS_BLOCK_HAS_PARAMS | CS_BLOCK_HAS_RESULT | CS_BLOCK_HAS_CORRELATOR;
                    
            // we must make sure we retain EID references to the existing
            // security-source and security-dest. Since this is a follower
            // correlated block, we don't have actual security-src and -dest
            // as those are set in the parent. 
                    
            // So now we have the encrypted block in the work buffer and what
            // remains to do is construct the actual block contents in place
            // of the plaintext.
                    
            // Note that we using OUR correlator here, not the one in the
            // original block
            correlator_size = SDNV::encoding_len(locals->correlator());
                    
            // First we need to work out the lengths and create the preamble
            //length = sizeof(num);         // ciphersuite number and flags
            length = 0;         // ciphersuite number and flags
            length += SDNV::encoding_len(CSNUM_PC3);
            length += SDNV::encoding_len(locals->cs_flags());
            length +=  correlator_size;
            param_len = 1 + 1 + iv_len;        // 8-byte iv, sdnv fits in 1 byte
            length += SDNV::encoding_len(param_len) + param_len;
            res_len = 1 + 1 + tag_len + encap_len;    //16-byte tag, sdnv is 1 byte
            length += SDNV::encoding_len(res_len) + res_len;
                    
            iter->writable_contents()->set_len(0);    // empty it to start with
            iter->set_owner(BundleProtocol::find_processor(BundleProtocol::CONFIDENTIALITY_BLOCK));            // "steal this block"
            generate_preamble(xmit_blocks, 
                              &*iter,
                              BundleProtocol::CONFIDENTIALITY_BLOCK,
                              //BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |  //This causes non-BSP nodes to delete the bundle
                              (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
                              length);
                    
                    
            log_debug_p(log, "Ciphersuite_PC3::finalize() preamble len %u block len %zu", iter->data_offset(), length);
            log_debug_p(log, "Ciphersuite_PC3::finalize() owner()->block_type() %u buf()[0] %hhu", 
                        iter->owner()->block_type(), iter->contents().buf()[0]);
            iter->writable_contents()->reserve(iter->data_offset() + length);
            iter->writable_contents()->set_len(iter->data_offset() + length);
            buf = iter->writable_contents()->buf() + iter->data_offset();
            len = length;
                    
            // Assemble data into block contents.
                        
            // ciphersuite number and flags
            new_target_locals->set_cs_flags(cs_flags);
            sdnv_len = SDNV::encode(CSNUM_PC3, buf, len);
            CS_FAIL_IF(sdnv_len <= 0); 
            buf += sdnv_len;
            len -= sdnv_len;
                    
            sdnv_len = SDNV::encode(new_target_locals->cs_flags(), buf, len);
            CS_FAIL_IF(sdnv_len <= 0); 
            buf += sdnv_len;
            len -= sdnv_len;
                    
            // correlator
            sdnv_len = SDNV::encode(locals->correlator(), buf, len);
            CS_FAIL_IF(sdnv_len <= 0); 
            buf += sdnv_len;
            len -= sdnv_len;
                    
            // length of security params
            sdnv_len = SDNV::encode(param_len, buf, len);
            CS_FAIL_IF(sdnv_len <= 0); 
            buf += sdnv_len;
            len -= sdnv_len;
                    
            // security params data - it's just the iv item
            *buf++ = CS_IV_field;
            --len;
            *buf++ = iv_len;
            --len;
            memcpy(buf, iv, iv_len);
            buf += iv_len;
            len -= iv_len;

            // length of security result
            sdnv_len = SDNV::encode(res_len, buf, len);
            CS_FAIL_IF(sdnv_len <= 0); 
            buf += sdnv_len;
            len -= sdnv_len;
                    
            // security result data - tag and the encapsulated block
            *buf++ = CS_PC_block_ICV_field;
            --len;
            *buf++ = tag_len;
            --len;
            memcpy(buf, tag, tag_len);
            buf += tag_len;
            len -= tag_len;
                    
                    
            memcpy(buf, encap_block.buf(), encap_block.len());
            buf += encap_block.len();
            len -= encap_block.len();
            CS_FAIL_IF(len != 0);
                    
            // fix up the BlockInfo and related things, 
            // remembering that "locals" was copied
            // from the original C3 block
                    
            iter->set_locals(new_target_locals);    //will also decrement ref for old one
            target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
            log_debug_p(log, "Ciphersuite_PC3::finalize() encapsulation done");

        }
        break;
            
        case BundleProtocol::PAYLOAD_BLOCK:
        {
            // prepare context -- key supplied already
            // nonce is 12 bytes, first 4 are salt (same for all blocks)
            // and last 8 bytes are per-block IV. The final 4 bytes in
            // the full block-sized field are, of course, the counter
            // which is not represented here
            u_char*            ptr;
            size_t            rem;
            u_char            type;
            u_int64_t        field_len;
            if(!bundle->payload_encrypted()) {
            ptr = nonce;
                    
            log_debug_p(log, "Ciphersuite_PC3::finalize() PAYLOAD_BLOCK");
            memcpy(ptr, locals->salt().buf(), salt_len);
            ptr += salt_len;
            memcpy(ptr, locals->iv().buf(), iv_len);
                    
            // prepare context
            log_debug_p(log, "Ciphersuite_PC3::finalize() nonce    0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
                        nonce[0], nonce[1], nonce[2], nonce[3], nonce[4], nonce[5], nonce[6], nonce[7], nonce[8], nonce[9], nonce[10], nonce[11]);
            gcm_init_message(nonce, nonce_len, &(ctx_ex.c));
                
            offset = iter->data_offset();
            len = iter->data_length();

            changed = 
                iter->owner()->mutate( Ciphersuite_PC3::do_crypt,
                                       deliberate_const_cast_bundle,
                                       block,
                                       &*iter,
                                       offset,
                                       len,
                                       r);
                    
            // collect the tag (icv) from the context
            gcm_compute_tag( tag, tag_len, &(ctx_ex.c) );
            log_debug_p(log, "Ciphersuite_PC3::finalize() tag      0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
                        tag[0], tag[1], tag[2], tag[3], tag[4], tag[5], tag[6], tag[7], tag[8], tag[9], tag[10], tag[11], tag[12], tag[13], tag[14], tag[15]);
             const_cast<Bundle *>(bundle)->set_payload_tag(tag);
             const_cast<Bundle *>(bundle)->set_payload_encrypted();
           }   else {
               memcpy(tag, bundle->payload_tag(), tag_len);
            log_debug_p(log, "Ciphersuite_PC3::finalize() tag      0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
                        tag[0], tag[1], tag[2], tag[3], tag[4], tag[5], tag[6], tag[7], tag[8], tag[9], tag[10], tag[11], tag[12], tag[13], tag[14], tag[15]);
          }

                    
            // get the result item, and step over the encrypted key item
            LocalBuffer* result = locals->writable_security_result();
            ptr = result->buf();
            rem = result->len();
            type = *ptr++;
            CS_FAIL_IF(type != Ciphersuite::CS_key_info_field);
            rem--;
            sdnv_len = SDNV::decode( ptr, rem, &field_len);
            ptr += sdnv_len;
            rem -= sdnv_len;
            ptr += field_len;
            rem -= field_len;
            CS_FAIL_IF( rem != 1 + 1 + tag_len);
            *ptr++ = CS_PC_block_ICV_field;
            rem--;
            *ptr++ = tag_len;
            rem--;
            memcpy(ptr, tag, tag_len);
                    
            // now put the result item into the block contents
            BlockInfo::DataBuffer* contents = block->writable_contents();
            u_char* buf = contents->buf();
            rem = contents->len();
            buf += block->data_offset();    // we need to add data_offset as well,
            rem -= block->data_offset();    // since we're pointing at the whole buffer
                    
            buf += locals->security_result_offset();    //and this offset is just within
            rem -= locals->security_result_offset();    //the data portion of the buffer
            sdnv_len = SDNV::len(buf);    // size of result-length field
            buf += sdnv_len;            // step over that length field
            rem -= sdnv_len;
            memcpy(buf, result->buf(), result->len());
            log_debug_p(log, "Ciphersuite_PC3::finalize() PAYLOAD_BLOCK done");
                    
                    
                    
        }
        break;  //break from switch, continue for "for" loop
                
        default:
            continue;
        
        }   // end of switch        
        
        
    }
    log_debug_p(log, "Ciphersuite_PC3::finalize() done");
    
    result = BP_SUCCESS;
    return result;

 fail:
    if ( locals !=  NULL )
        locals->set_proc_flag(CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
bool
Ciphersuite_PC3::do_crypt(const Bundle*    bundle,
                         const BlockInfo* caller_block,
                         BlockInfo*       target_block,
                         void*            buf,
                         size_t           len,
                         OpaqueContext*   r)
{    
    (void) bundle;
    (void) caller_block;
    (void) target_block;
    gcm_ctx_ex* pctx = reinterpret_cast<gcm_ctx_ex*>(r);

    log_debug_p(log, "Ciphersuite_PC3::do_crypt() operation %hhu len %zu", pctx->operation, len);

    if (pctx->operation == op_encrypt)
        gcm_encrypt( reinterpret_cast<u_char*>(buf), len, &(pctx->c) );
    else    
        gcm_decrypt( reinterpret_cast<u_char*>(buf), len, &(pctx->c) );

    return (len > 0) ? true : false;
}

} // namespace dtn

#endif /* BSP_ENABLED */

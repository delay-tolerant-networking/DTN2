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

#include "Ciphersuite_ES4.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "bundling/MetadataBlockProcessor.h"
#include "bundling/MetadataBlock.h"
#include "bundling/SDNV.h"
#include "contacts/Link.h"
#include "openssl/rand.h"
#include "gcm/gcm.h"
#include "security/KeySteward.h"

namespace dtn {

static const char * log = "/dtn/bundle/ciphersuite";

//----------------------------------------------------------------------
Ciphersuite_ES4::Ciphersuite_ES4()
{
}

//----------------------------------------------------------------------
u_int16_t
Ciphersuite_ES4::cs_num(void)
{
    return CSNUM_ES4;
}

//----------------------------------------------------------------------
int
Ciphersuite_ES4::consume(Bundle* bundle, BlockInfo* block,
                        u_char* buf, size_t len)
{
    log_debug_p(log, "Ciphersuite_ES4::consume()");
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
Ciphersuite_ES4::validate(const Bundle*          bundle,
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
    u_int16_t       cs_flags =0;
    BP_Local_CS*    locals = NULL;
    EndpointID      local_eid = BundleDaemon::instance()->local_eid();
    size_t          len=0;
    gcm_ctx_ex      ctx_ex;             // includes OpenSSL context within it
    u_char          key[key_len];       //use AES128 16-byte key
    u_char          salt[salt_len];     // salt for GCM
    u_char          iv[iv_len];         // GCM "iv" length is 8 bytes
    u_char          nonce[nonce_len];   // 12 bytes recommended
    u_char          tag[tag_len];       // 128 bits recommended
    u_char*         buf=NULL;
    u_char*         ptr=NULL;
    u_char*         data=NULL;
    int             sdnv_len = 0;       // use an int to handle -1 return values
    u_char          item_type;
    int32_t         rem=0;                // use signed value
    u_int64_t       field_length = 0LL;
    u_int64_t       frag_offset_=0;       // Offset of fragment in the original bundle
    u_int64_t       orig_length_=0;       // Length of original bundle
    ret_type        ret = 0;
    DataBuffer      db;
    DataBuffer      encap_block;        // Holds the encrypted, encapsulated extension block

    log_debug_p(log, "Ciphersuite_ES4::validate() %p", block);

    // there may be several ESB blocks to decrypt
    while (block = contains_ESB_block(block_list)) {

    	locals = dynamic_cast<BP_Local_CS*>(block->locals());
    	CS_FAIL_IF_NULL(locals);
    	cs_flags = locals->cs_flags();

    	if ( Ciphersuite::destination_is_local_node(bundle, block) )
    	{  //yes - this is ours so go to work

    		// get pieces from params -- salt, iv, range,
    		buf = locals->security_params().buf();
    		len = locals->security_params().len();

    		log_debug_p(log, "Ciphersuite_ES4::validate() security params, len = %zu", len);
    		while ( len > 0 ) {
    			item_type = *buf++;
    			--len;
    			sdnv_len = SDNV::decode(buf, len, &field_length);
    			buf += sdnv_len;
    			len -= sdnv_len;

    			switch ( item_type ) {
    			case CS_IV_field:
    			{
    				log_debug_p(log, "Ciphersuite_ES4::validate() iv item, len = %llu", U64FMT(field_length));
    				memcpy(iv, buf, iv_len);
    				buf += field_length;
    				len -= field_length;
    			}
    			break;

    			case CS_PC_block_salt:
    			{
    				log_debug_p(log, "Ciphersuite_ES4::validate() salt item, len = %llu", U64FMT(field_length));
    				memcpy(salt, buf, nonce_len - iv_len);
    				buf += field_length;
    				len -= field_length;
    			}
    			break;

    			case CS_fragment_offset_and_length_field:
    			{
    				log_debug_p(log, "Ciphersuite_ES4::validate() frag info item, len = %llu", U64FMT(field_length));
    				sdnv_len = SDNV::decode(buf, len, &frag_offset_);
    				buf += sdnv_len;
    				len -= sdnv_len;
    				sdnv_len = SDNV::decode(buf, len, &orig_length_);
    				buf += sdnv_len;
    				len -= sdnv_len;
    			}
    			break;

    			case CS_key_info_field:
    			{
    				log_debug_p(log, "Ciphersuite_ES4::validate() key-info item");

    				log_debug_p(log, "Ciphersuite_ES4::validate() key-info (1st 20 bytes) : 0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
    						buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
    						buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15],
    						buf[16], buf[17], buf[18], buf[19]);

    				// Decrypt the RSA-encrypted Bundle-Encryption Key (BEK)
    				KeySteward::decrypt(bundle, locals->security_src(), buf, field_length, db, (int) cs_num());
    				memcpy(key, db.buf(), key_len);
    				log_debug_p(log, "Ciphersuite_ES4::validate() key 0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
    						key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7],
    						key[8], key[9], key[10], key[11], key[12], key[13], key[14], key[15]);

    				buf += field_length;
    				len -= field_length;
    			}
    			break;

    			default:    // deal with improper items
    				log_err_p(log, "Ciphersuite_ES4::validate: unexpected item type %d in security_params",
    						item_type);
    				goto fail;
    			}
    		}

    		// get pieces from the security result
    		buf = locals->security_result().buf();
    		len = locals->security_result().len();

    		log_debug_p(log, "Ciphersuite_ES4::validate() security result, len = %zu", len);
    		while ( len > 0 ) {
    			item_type = *buf++;
    			--len;
    			sdnv_len = SDNV::decode(buf, len, &field_length);
    			buf += sdnv_len;
    			len -= sdnv_len;

    			switch ( item_type ) {
    			case CS_PC_block_ICV_field:
    			{
    				log_debug_p(log, "Ciphersuite_ES4::validate() icv item");
    				memcpy(tag, buf, tag_len);

    				log_debug_p(log, "Ciphersuite_ES4::validate() tag      0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
    						tag[0], tag[1], tag[2], tag[3], tag[4], tag[5], tag[6], tag[7], tag[8],
    						tag[9], tag[10], tag[11], tag[12], tag[13], tag[14], tag[15]);

    				buf += field_length;
    				len -= field_length;
    			}
    			break;

    			case CS_encap_block_field:
    			{
    				// Encrypted extension block
    				log_debug_p(log, "Ciphersuite_ES4::validate() encap block item, len = %llu", U64FMT(field_length));
    				encap_block.reserve(field_length);
    				encap_block.set_len(field_length);
    				memcpy(encap_block.buf(), buf, field_length);
    				buf += field_length;
    				len -= field_length;
    			}
    			break;

    			default:    // deal with improper items
    				log_err_p(log, "Ciphersuite_ES4::validate: unexpected item type %d in security_result",
    						item_type);
    				goto fail;
    			}  // end switch
    		}  // end while

    		// prepare context - one time for all usage here
    		gcm_init_and_key(key, key_len, &(ctx_ex.c));
    		ctx_ex.operation = op_decrypt;

    		// nonce is 12 bytes, first 4 are salt, last 8 are IV
    		ptr = nonce;
    		memcpy(ptr, salt, nonce_len - iv_len);
    		ptr += nonce_len - iv_len;
    		memcpy(ptr, iv, iv_len);

    		// prepare context
    		gcm_init_message(nonce, nonce_len, &(ctx_ex.c));

    		// decrypt message
    		ret = gcm_decrypt_message(nonce,
    				nonce_len,
    				NULL,
    				0,
    				encap_block.buf(),
    				encap_block.len(),
    				tag,                // tag (ICV) is input, for validation against calculated tag
    				tag_len,
    				&(ctx_ex.c));

    		// check return value (verify that message decrypted correctly)
    		if ( ret != 0 ) {
    			log_err_p(log, "Ciphersuite_ES4::validate: gcm_decrypt_message failed, ret = %d", ret);
    			goto fail;
    		}

    		// encap_block is the raw data of the encapsulated block
    		// and now we have to reconstitute it the way it used to be :)
    		// Parse the content as would be done for a newly-received block
    		// using the owner's consume() method
    		data = encap_block.buf();
    		len = encap_block.len();

    		BlockInfo info(BundleProtocol::find_processor(*data));
    		u_int64_t flags;
    		u_int64_t content_length = 0LLU;

    		BlockInfo::DataBuffer   preamble;
    		preamble.reserve(len);  // the preamble can't be bigger than the entire block

    		// copy bits and pieces from the decrypted block
    		ptr = preamble.buf();
    		rem = len;

    		*ptr++ = *data++;  // record the block type
    		rem--;
    		len--;
    		sdnv_len = SDNV::decode(data, len, &flags);  // block processing flags (SDNV)
    		data += sdnv_len;
    		len -= sdnv_len;
    		log_debug_p(log, "Ciphersuite_ES4::validate() target block type %hhu flags 0x%llx", *(preamble.buf()), U64FMT(flags));

    		sdnv_len = SDNV::decode(data, len, &content_length);
    		data += sdnv_len;
    		len -= sdnv_len;
    		log_debug_p(log, "Ciphersuite_ES4::validate() target data content size %llu", U64FMT(content_length));

    		// fix up last-block flag
    		// this probably isn't the last block, but who knows ? :)
    		if ( block->flags() & BundleProtocol::BLOCK_FLAG_LAST_BLOCK )
    			flags |= BundleProtocol::BLOCK_FLAG_LAST_BLOCK;
    		else
    			flags &= ~BundleProtocol::BLOCK_FLAG_LAST_BLOCK;

    		// put flags into the adjusted block
    		sdnv_len = SDNV::encode(flags, ptr, rem);
    		ptr += sdnv_len;
    		rem -= sdnv_len;

    		// length of data content in block
    		sdnv_len = SDNV::encode(content_length, ptr, rem);
    		ptr += sdnv_len;
    		rem -= sdnv_len;

    		// we now have a preamble in "preamble" and the rest of the data at *data
    		size_t    preamble_size = ptr - preamble.buf();
    		preamble.set_len(preamble_size);
    		log_debug_p(log, "Ciphersuite_ES4::validate() target preamble_size %zu", preamble_size);

    		{
    			// we're reusing the existing BlockInfo but we need to clean it first
    			info.~BlockInfo();
    			// we'd like to reinitilize the block thusly
    			// info.BlockInfo(type);
    			// but C++ gets bent so we have to achieve the desired result
    			// in a more devious fashion using placement-new.

    			log_debug_p(log, "Ciphersuite_ES4::validate() re-init target");
    			BlockInfo* bp = &info;
    			bp = new (bp) BlockInfo(BundleProtocol::find_processor(*(preamble.buf())));
    			CS_FAIL_IF_NULL(bp);
    		}

    		// process preamble
    		log_debug_p(log, "Ciphersuite_ES4::validate() process target preamble");
    		int cc = info.owner()->consume(deliberate_const_cast_bundle, &info, preamble.buf(), preamble_size);
    		if (cc < 0) {
    			log_err_p(log, "Ciphersuite_ES4::validate: consume failed handling encapsulated preamble 0x%x, cc = %d",
    					info.type(), cc);
    			goto fail;
    		}

    		// process the main part of the encapsulated block
    		log_debug_p(log, "Ciphersuite_ES4::validate() process target content");

    		cc = info.owner()->consume(deliberate_const_cast_bundle, &info, data, len);
    		if (cc < 0) {
    			log_err_p(log, "Ciphersuite_ES4::validate: consume failed handling encapsulated block 0x%x, cc = %d",
    					info.type(), cc);
    			goto fail;
    		}
    		log_debug_p(log, "Ciphersuite_ES4::validate() decapsulation done");

    		// Replace the ESB block with the new (decapsulated) block
    		for (BlockInfoVec::iterator iter = block_list->begin(); iter != block_list->end(); ++iter) {
    			if (iter->type() == block->type()) *iter = info;
    		}

    	}
    	else
    		locals->set_proc_flag(CS_BLOCK_DID_NOT_FAIL);   // not for here so we didn't check this block
    } // end while

    log_debug_p(log, "Ciphersuite_ES4::validate() %p done", block);
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
Ciphersuite_ES4::prepare(const Bundle*    bundle,
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
    	log_debug_p(log, "Ciphersuite_ES4::prepare() - not being forwarded");
    	return BP_SUCCESS;     // it was for us so don't forward
    }

    BlockInfo bi = BlockInfo(BundleProtocol::find_processor(BundleProtocol::EXTENSION_SECURITY_BLOCK), source); // NULL source is OK here

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
    	log_debug_p(log, "Ciphersuite_ES4::prepare() - forward received block len %u eid_list_count %zu new count %zu",
    			source->full_length(), source->eid_list().size(), bp->eid_list().size());

    	CS_FAIL_IF_NULL( source->locals() )       // broken

    	source_locals = dynamic_cast<BP_Local_CS*>(source->locals());
    	CS_FAIL_IF_NULL(source_locals);
    	bp->set_locals(new BP_Local_CS);
    	locals = dynamic_cast<BP_Local_CS*>(bp->locals());
    	CS_FAIL_IF_NULL(locals);
    	locals->set_owner_cs_num(CSNUM_ES4);
    	cs_flags = source_locals->cs_flags();
    	locals->set_list_owner(BlockInfo::LIST_RECEIVED);
    	locals->set_correlator(source_locals->correlator());
    	bp->writable_contents()->reserve(source->full_length());
    	bp->writable_contents()->set_len(0);

    	// copy security-src and -dest if they exist
    	if ( source_locals->cs_flags() & CS_BLOCK_HAS_SOURCE ) {
    		CS_FAIL_IF(source_locals->security_src().length() == 0 );
    		log_debug_p(log, "Ciphersuite_ES4::prepare() add security_src EID");
    		cs_flags |= CS_BLOCK_HAS_SOURCE;
    		locals->set_security_src(source_locals->security_src());
    	}

    	if ( source_locals->cs_flags() & CS_BLOCK_HAS_DEST ) {
    		CS_FAIL_IF(source_locals->security_dest().length() == 0 );
    		log_debug_p(log, "Ciphersuite_ES4::prepare() add security_dest EID");
    		cs_flags |= CS_BLOCK_HAS_DEST;
    		locals->set_security_dest(source_locals->security_dest());
    	}
    	locals->set_cs_flags(cs_flags);
    	log_debug_p(log, "Ciphersuite_ES4::prepare() - inserted block eid_list_count %zu",
    			bp->eid_list().size());
    	result = BP_SUCCESS;
    	return result;
    } 
    else {
    	// initialize the block
    	log_debug_p(log, "Ciphersuite_ES4::prepare() - add new block (or API block etc)");
    	bi.set_locals(new BP_Local_CS);
    	CS_FAIL_IF_NULL(bi.locals());
    	locals = dynamic_cast<BP_Local_CS*>(bi.locals());
    	CS_FAIL_IF_NULL(locals);
    	locals->set_owner_cs_num(CSNUM_ES4);
    	locals->set_list_owner(list);

    	// if there is a security-src and/or -dest, use it -- might be specified by API
    	if ( source != NULL && source->locals() != NULL)  {
    		locals->set_security_src(dynamic_cast<BP_Local_CS*>(source->locals())->security_src());
    		locals->set_security_dest(dynamic_cast<BP_Local_CS*>(source->locals())->security_dest());
    	}

    	log_debug_p(log, "Ciphersuite_ES4::prepare() local_eid %s bundle->source_ %s", local_eid.c_str(), bundle->source().c_str());
    	// if not, and we didn't create the bundle, specify ourselves as sec-src
    	if ( (locals->security_src().length() == 0) && (local_eid != bundle->source()))
    		locals->set_security_src(local_eid.str());

    	// if we now have one, add it to list, etc
    	if ( locals->security_src().length() > 0 ) {
    		log_debug_p(log, "Ciphersuite_ES4::prepare() add security_src EID %s", locals->security_src().c_str());
    		cs_flags |= CS_BLOCK_HAS_SOURCE;
    		bi.add_eid(locals->security_src());
    	}

    	if ( locals->security_dest().length() > 0 ) {
    		log_debug_p(log, "Ciphersuite_ES4::prepare() add security_dest EID %s", locals->security_dest().c_str());
    		cs_flags |= CS_BLOCK_HAS_DEST;
    		bi.add_eid(locals->security_dest());
    	}

    	locals->set_cs_flags(cs_flags);
    	log_debug_p(log, "Ciphersuite_ES4::prepare() ciphersuite flags set");

    	// We should already have the primary block in the list.
    	// We'll insert the ESB block after the primary, payload, and
    	// any other security blocks, but before anything else
    	if ( xmit_blocks->size() > 0 ) {
    		BlockInfoVec::iterator iter = xmit_blocks->begin();

    		while ( iter != xmit_blocks->end()) {

    			switch (iter->type()) {
    			case BundleProtocol::PRIMARY_BLOCK:
    			case BundleProtocol::PAYLOAD_BLOCK:
    			case BundleProtocol::CONFIDENTIALITY_BLOCK:
    			case BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK:
    			case BundleProtocol::PAYLOAD_SECURITY_BLOCK:
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
Ciphersuite_ES4::generate(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link,
                         bool           last)
{
    (void)bundle;
    (void)link;
    (void)xmit_blocks;
    
    int             result = BP_FAIL;
    u_int16_t       cs_flags = 0;
    BP_Local_CS*    locals = dynamic_cast<BP_Local_CS*>(block->locals());
    DataBuffer      encrypted_key;

    log_debug_p(log, "Ciphersuite_ES4::generate() %p", block);

    CS_FAIL_IF_NULL(locals);
    cs_flags = locals->cs_flags();        // get flags from prepare()

    // if this is a received block then it's easy
    if ( locals->list_owner() == BlockInfo::LIST_RECEIVED ) 
    {
    	// generate the preamble and copy the data.
    	size_t length = block->source()->data_length();

    	generate_preamble(xmit_blocks,
    			block,
    			BundleProtocol::EXTENSION_SECURITY_BLOCK,
                //BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |  //This causes non-BSP nodes to delete the bundle
    			BundleProtocol::BLOCK_FLAG_REPLICATE           |
    			(last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
    			length);

    	BlockInfo::DataBuffer* contents = block->writable_contents();
    	contents->reserve(block->data_offset() + length);
    	contents->set_len(block->data_offset() + length);
    	memcpy(contents->buf() + block->data_offset(),
    			block->source()->data(), length);
    	log_debug_p(log, "Ciphersuite_ES4::generate() %p done", block);
    	return BP_SUCCESS;
    }

    // need to fill in the preamble for the ESB block
    generate_preamble(xmit_blocks,
    		block,
    		BundleProtocol::EXTENSION_SECURITY_BLOCK,
            //BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |  //This causes non-BSP nodes to delete the bundle
    		(last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
    		10);

    result = BP_SUCCESS;
    return result;

    fail:
    if ( locals !=  NULL )
    	locals->set_proc_flag(CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
    return BP_FAIL;
}

//----------------------------------------------------------------------
int
Ciphersuite_ES4::finalize(const Bundle*  bundle, 
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     old_ESB_block,
                         const LinkRef& link)
{
	gcm_ctx_ex      ctx_ex;                   // includes OpenSSL context within it
	u_char          key[key_len];             // AES-128 (16-byte key)
	u_char          nonce[nonce_len];         // 12 bytes recommended
	u_char          iv[iv_len];               // AES iv length
	u_char          salt[nonce_len - iv_len]; // salt for GCM
	u_char          tag[tag_len];             // 128 bits recommended
	BP_Local_CS*    locals = NULL;
	EndpointID      local_eid = BundleDaemon::instance()->local_eid();
	u_int16_t       cs_flags = 0;
	DataBuffer      encrypted_key;
	int             err = 0;
	DataBuffer*     contents = NULL;
	LocalBuffer*    security_result = NULL;
	LocalBuffer*    params = NULL;
	size_t          param_len = 0;
	size_t          result_len = 0;
	size_t          tot_len = 0;
	int             sdnv_len = 0;              // use an int to handle -1 return values
	u_char*         ptr;
	size_t          rem;
    BP_LocalRef  localsref("Ciphersuite_ES4::finalize"); // we need to convince oasys not to garbage collect our locals


	log_debug_p(log, "Ciphersuite_ES4::finalize()");
	locals = dynamic_cast<BP_Local_CS*>(old_ESB_block->locals());
    localsref = locals;
	CS_FAIL_IF_NULL(locals);

	// if this is a received block then we're done
	if ( locals->list_owner() == BlockInfo::LIST_RECEIVED )
		return BP_SUCCESS;

	// delete the ESB block (we'll add a new ESB block for each extension block)
	log_debug_p(log, "Ciphersuite_ES4::finalize() walk block list");
	for (BlockInfoVec::iterator iter = xmit_blocks->begin();
			iter != xmit_blocks->end();
			++iter) {
		if (iter->type() == BundleProtocol::EXTENSION_SECURITY_BLOCK) {
			xmit_blocks->erase(iter);
			break;
		}
	}

	// generate actual key
	RAND_bytes(key, sizeof(key));

	// encrypt the key with RSA
	log_debug_p(log, "Ciphersuite_ES4::finalize() key      0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
			key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7],
			key[8], key[9], key[10], key[11], key[12], key[13], key[14], key[15]);

	err = KeySteward::encrypt(bundle, NULL, link, locals->security_dest(), key, sizeof(key), encrypted_key, (int) cs_num());
	CS_FAIL_IF(err != 0);

	// prepare context - one time for all usage here
	gcm_init_and_key(key, key_len, &(ctx_ex.c));
	ctx_ex.operation = op_encrypt;

	// loop thru block list to replace all extension blocks
	while (contains_extension_block(xmit_blocks)) {

		log_debug_p(log, "Ciphersuite_ES4::finalize() walk block list");
		for (BlockInfoVec::iterator iter = xmit_blocks->begin();
				iter != xmit_blocks->end();
				++iter)
		{
			switch ( iter->type() ) {

			// skip primary, payload, security blocks, or previous hop blocks
			case BundleProtocol::PRIMARY_BLOCK:
			case BundleProtocol::PAYLOAD_BLOCK:
			case BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK:
			case BundleProtocol::CONFIDENTIALITY_BLOCK:
			case BundleProtocol::PAYLOAD_SECURITY_BLOCK:
			case BundleProtocol::EXTENSION_SECURITY_BLOCK:
			case BundleProtocol::PREVIOUS_HOP_BLOCK:
				continue;

				// Any remaining block must be some type of extension block
				// We want to replace this with an ESB block
			default:
			{
				log_debug_p(log, "Ciphersuite_ES4::finalize() replace this block with extension block, len %u eid_ref_count %zu",
						iter->full_length(), iter->eid_list().size());

				BlockInfo* ext_block = (BlockInfo*) &(*iter);

				// create a new ESB block to replace the extension block
				BlockInfo new_ESB_block = BlockInfo(BundleProtocol::find_processor(
						BundleProtocol::EXTENSION_SECURITY_BLOCK), ext_block);

				// initialize the new ESB block
				new_ESB_block.set_locals(new BP_Local_CS);
				locals = dynamic_cast<BP_Local_CS*>(new_ESB_block.locals());
				CS_FAIL_IF_NULL(locals);
				locals->set_owner_cs_num(CSNUM_ES4);
				locals->set_list_owner(BlockInfo::LIST_NONE);

				// if there is a security-src and/or -dest, use it -- might be specified by API
				if ( ext_block != NULL && ext_block->locals() != NULL)  {
					locals->set_security_src(dynamic_cast<BP_Local_CS*>(ext_block->locals())->security_src());
					locals->set_security_dest(dynamic_cast<BP_Local_CS*>(ext_block->locals())->security_dest());
				}

				log_debug_p(log, "Ciphersuite_ES4::finalize() local_eid %s bundle->source_ %s", local_eid.c_str(), bundle->source().c_str());
				// if not, and we didn't create the bundle, specify ourselves as sec-src
				if ( (locals->security_src().length() == 0) && (local_eid != bundle->source()))
					locals->set_security_src(local_eid.str());

				// if we now have one, add it to list, etc
				if ( locals->security_src().length() > 0 ) {
					log_debug_p(log, "Ciphersuite_ES4::finalize() add security_src EID %s", locals->security_src().c_str());
					cs_flags |= CS_BLOCK_HAS_SOURCE;
					new_ESB_block.add_eid(locals->security_src());
				}
				if ( locals->security_dest().length() > 0 ) {
					log_debug_p(log, "Ciphersuite_ES4::finalize() add security_dest EID %s", locals->security_dest().c_str());
					cs_flags |= CS_BLOCK_HAS_DEST;
					new_ESB_block.add_eid(locals->security_dest());
				}
				cs_flags |= CS_BLOCK_HAS_PARAMS;
				cs_flags |= CS_BLOCK_HAS_RESULT;
				locals->set_cs_flags(cs_flags);

				// generate 12-byte nonce for this extension block
				// first 4 bytes of nonce are the salt
				ptr = nonce;
				RAND_bytes(salt, sizeof(salt));
				memcpy(ptr, salt, salt_len);
				ptr += salt_len;

				// last 8 bytes of nonce are the IV
				RAND_bytes(iv, sizeof(iv));
				memcpy(ptr, iv, iv_len);

				// prepare context
				log_debug_p(log, "Ciphersuite_ES4::finalize() nonce    0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
						nonce[0], nonce[1], nonce[2], nonce[3], nonce[4], nonce[5], nonce[6], nonce[7], nonce[8], nonce[9], nonce[10], nonce[11]);

				/* fill in the security-parameters field
                   params field will contain
                   - salt (4 bytes), plus type and length
                   - IV (block-length, 8 bytes), plus type and length
                   - encrypted key, plus type and length
                   - fragment offset and length, if a fragment-bundle, plus type and length
                   - key-identifier (optional, not implemented yet), plus type and length
				 */
				params = locals->writable_security_params();
				param_len = 1 + 1 + sizeof(salt);
				param_len += 1 + 1 + sizeof(iv);

				param_len += 1 + SDNV::encoding_len(encrypted_key.len()) + encrypted_key.len();

				params->reserve(param_len);
				params->set_len(param_len);
				log_debug_p(log, "Ciphersuite_ES4::finalize() security params, len = %zu", param_len);
				rem = param_len;

				// copy the salt and IV into the params field
				ptr = params->buf();
				*ptr++ = CS_PC_block_salt;
				rem--;
				*ptr++ = sizeof(salt);                // less than 127
				rem--;
				memcpy(ptr, salt, sizeof(salt));
				ptr += sizeof(salt);
				rem -= sizeof(salt);
				*ptr++ = CS_IV_field;
				rem--;
				*ptr++ = sizeof(iv);                // less than 127
				rem--;
				memcpy(ptr, iv, sizeof(iv));
				ptr += sizeof(iv);
				rem -= sizeof(iv);

				// copy the encrypted key into the params field
				*ptr++ = Ciphersuite::CS_key_info_field;
				rem--;
				sdnv_len = SDNV::encode(encrypted_key.len(), ptr, rem);
				ptr += sdnv_len;
				rem -= sdnv_len;
				memcpy(ptr, encrypted_key.buf(), encrypted_key.len());
				ptr += encrypted_key.len();
				rem -= encrypted_key.len();

				// initialize the context with our nonce
				gcm_init_message(nonce, nonce_len, &(ctx_ex.c));

				// encrypt extension block in-place
				gcm_encrypt_message(nonce,
						nonce_len,
						NULL,
						0,
						iter->writable_contents()->buf(),
						iter->full_length(),
						tag,
						tag_len,
						&(ctx_ex.c));

				log_debug_p(log, "Ciphersuite_ES4::finalize() tag      0x%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx%2.2hhx",
						tag[0], tag[1], tag[2], tag[3], tag[4], tag[5], tag[6], tag[7], tag[8],
						tag[9], tag[10], tag[11], tag[12], tag[13], tag[14], tag[15]);

				/* fill in the result field
                   result field will contain
                   - ICV (Integrity Check Value), plus type and length
                   - encapsulated extension block
				 */
				result_len = 1 + 1 + tag_len;
				result_len += 1 + SDNV::encoding_len(iter->full_length()) + iter->full_length();

				security_result = locals->writable_security_result();
				security_result->reserve(result_len);
				security_result->set_len(result_len);
				ptr = security_result->buf();
				rem = result_len;

				*ptr++ = CS_PC_block_ICV_field;
				rem--;
				*ptr++ = tag_len;
				rem--;

				memcpy(ptr, tag, tag_len);
				ptr += tag_len;
				rem -= tag_len;

				// after the ICV is the encapsulated extension block
				*ptr++ = CS_encap_block_field;
				sdnv_len = SDNV::encode(iter->full_length(), ptr, rem);
				CS_FAIL_IF(sdnv_len <= 0);
				ptr += sdnv_len;
				memcpy(ptr, iter->writable_contents()->buf(), iter->full_length());

				// before we can create the preamble, need to determine total length
				tot_len = 0;
				tot_len += SDNV::encoding_len(CSNUM_ES4);
				tot_len += SDNV::encoding_len(locals->cs_flags());
				tot_len += SDNV::encoding_len(param_len) + param_len;
				tot_len += SDNV::encoding_len(result_len) + result_len;

				// the new ESB block will be the last block if the extension block is last
				bool last = false;
				if (iter == xmit_blocks->end()) last = true;

				generate_preamble(xmit_blocks,
						&new_ESB_block,
						BundleProtocol::EXTENSION_SECURITY_BLOCK,
                        //BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |  //This causes non-BSP nodes to delete the bundle
						(last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
						tot_len);

				log_debug_p(log, "Ciphersuite_ES4::generate() preamble len %u block len %zu", new_ESB_block.data_offset(), tot_len);

				contents = new_ESB_block.writable_contents();
				contents->reserve(new_ESB_block.data_offset() + tot_len);
				contents->set_len(new_ESB_block.data_offset() + tot_len);

				ptr = contents->buf() + new_ESB_block.data_offset();
				rem = tot_len;

				// ciphersuite number and flags
				sdnv_len = SDNV::encode(locals->owner_cs_num(), ptr, rem);
				CS_FAIL_IF(sdnv_len <= 0);
				ptr += sdnv_len;
				rem -= sdnv_len;

				sdnv_len = SDNV::encode(locals->cs_flags(), ptr, rem);
				CS_FAIL_IF(sdnv_len <= 0);
				ptr += sdnv_len;
				rem -= sdnv_len;

				// length of params
				sdnv_len = SDNV::encode(param_len, ptr, rem);
				CS_FAIL_IF(sdnv_len <= 0);
				ptr += sdnv_len;
				rem -= sdnv_len;

				// params data
				memcpy(ptr, params->buf(), param_len);
				ptr += param_len;
				rem -= param_len;

				// length of security result
				sdnv_len = SDNV::encode(result_len, ptr, rem);
				CS_FAIL_IF(sdnv_len <= 0);
				ptr += sdnv_len;
				rem -= sdnv_len;

				// security result data
				memcpy(ptr, security_result->buf(), result_len);
				ptr += result_len;
				rem -= result_len;

				xmit_blocks->insert(iter, new_ESB_block);
				iter++;

				// now we need to remove the original extension block
				xmit_blocks->erase(iter);
			}  // end default
			}  // end switch
		}   // end for
	}  // end while
	log_debug_p(log, "Ciphersuite_ES4::finalize() done");
	return BP_SUCCESS;

	fail:
	if ( locals !=  NULL )
		locals->set_proc_flag(CS_BLOCK_PROCESSING_FAILED_DO_NOT_SEND);
	return BP_FAIL;
}

//----------------------------------------------------------------------
bool
Ciphersuite_ES4::contains_extension_block(BlockInfoVec* xmit_blocks)
{
	int block_type;

	// walk block list
	for (BlockInfoVec::iterator iter = xmit_blocks->begin();
			iter != xmit_blocks->end();
			++iter) {

		block_type = iter->type();
		if ((block_type != BundleProtocol::PRIMARY_BLOCK) &&
				(block_type != BundleProtocol::PAYLOAD_BLOCK) &&
				(block_type != BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK) &&
				(block_type != BundleProtocol::CONFIDENTIALITY_BLOCK) &&
				(block_type != BundleProtocol::PAYLOAD_SECURITY_BLOCK) &&
				(block_type != BundleProtocol::EXTENSION_SECURITY_BLOCK) &&
				(block_type != BundleProtocol::PREVIOUS_HOP_BLOCK) ) {
			return true;
		}

	}
	return false;
}


//----------------------------------------------------------------------
BlockInfo*
Ciphersuite_ES4::contains_ESB_block(BlockInfoVec* block_list)
{
	// walk block list
	for (BlockInfoVec::iterator iter = block_list->begin();
			iter != block_list->end();
			++iter) {
		if (iter->type() == BundleProtocol::EXTENSION_SECURITY_BLOCK) {
			BlockInfo* new_ESB_block = (BlockInfo*) &(*iter);
			return new_ESB_block;
		}
	}
	return false;
}


//----------------------------------------------------------------------
bool
Ciphersuite_ES4::do_crypt(const Bundle*    bundle,
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
    
    log_debug_p(log, "Ciphersuite_ES4::do_crypt() operation %hhu len %zu", pctx->operation, len);
    if (pctx->operation == op_encrypt)
        gcm_encrypt( reinterpret_cast<u_char*>(buf), len, &(pctx->c) );
    else    
        gcm_decrypt( reinterpret_cast<u_char*>(buf), len, &(pctx->c) );

    return (len > 0) ? true : false;
}

} // namespace dtn

#endif /* BSP_ENABLED */

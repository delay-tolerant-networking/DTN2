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

#include <string>
#include <sys/types.h>
#include <netinet/in.h>

#include "Bundle.h"
#include "BundleProtocol.h"
#include "PrimaryBlockProcessor.h"
#include "naming/EndpointID.h"
#include "SDNV.h"

namespace dtn {

//----------------------------------------------------------------------
struct DictionaryEntry {
    DictionaryEntry(const std::string& s, size_t off)
	: str(s), offset(off) {}

    std::string str;
    size_t offset;
};

class DictionaryVector : public std::vector<DictionaryEntry> {};

//----------------------------------------------------------------------
PrimaryBlockProcessor::PrimaryBlockProcessor()
    : BlockProcessor(BundleProtocol::PRIMARY_BLOCK)
{
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::add_to_dictionary(const EndpointID& eid,
                                         DictionaryVector* dict,
                                         size_t* dictlen)
{
    /*
     * For the scheme and ssp parts of the given endpoint id, see if
     * they've already appeared in the vector. If not, add them, and
     * record their length (with the null terminator) in the running
     * length total.
     */
    DictionaryVector::iterator iter;
    bool found_scheme = false;
    bool found_ssp = false;

    for (iter = dict->begin(); iter != dict->end(); ++iter) {
	if (iter->str == eid.scheme_str())
	    found_scheme = true;

	if (iter->str == eid.ssp())
	    found_ssp = true;
    }

    if (found_scheme == false) {
	dict->push_back(DictionaryEntry(eid.scheme_str(), *dictlen));
	*dictlen += (eid.scheme_str().length() + 1);
    }

    if (found_ssp == false) {
	dict->push_back(DictionaryEntry(eid.ssp(), *dictlen));
	*dictlen += (eid.ssp().length() + 1);
    }
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::get_dictionary_offsets(DictionaryVector *dict,
                                              const EndpointID& eid,
                                              u_int16_t* scheme_offset,
                                              u_int16_t* ssp_offset)
{
    DictionaryVector::iterator iter;
    for (iter = dict->begin(); iter != dict->end(); ++iter) {
	if (iter->str == eid.scheme_str())
	    *scheme_offset = htons(iter->offset);

	if (iter->str == eid.ssp())
	    *ssp_offset = htons(iter->offset);
    }
}

//----------------------------------------------------------------------
bool
PrimaryBlockProcessor::extract_dictionary_eid(EndpointID* eid,
                                              const char* what,
                                              u_int16_t* scheme_offsetp,
                                              u_int16_t* ssp_offsetp,
                                              u_char* dictionary,
                                              u_int32_t dictionary_len)
{
    static const char* log = "/dtn/bundle/protocol";
    u_int16_t scheme_offset, ssp_offset;
    memcpy(&scheme_offset, scheme_offsetp, 2);
    memcpy(&ssp_offset, ssp_offsetp, 2);
    scheme_offset = ntohs(scheme_offset);
    ssp_offset = ntohs(ssp_offset);

    if (scheme_offset >= (dictionary_len - 1)) {
	log_err(log, "illegal offset for %s scheme dictionary offset: "
		"offset %d, total length %u", what,
		scheme_offset, dictionary_len);
	return false;
    }

    if (ssp_offset >= (dictionary_len - 1)) {
	log_err(log, "illegal offset for %s ssp dictionary offset: "
		"offset %d, total length %u", what,
		ssp_offset, dictionary_len);
	return false;
    }
    
    eid->assign((char*)&dictionary[scheme_offset],
                (char*)&dictionary[ssp_offset]);

    if (! eid->valid()) {
	log_err(log, "invalid %s endpoint id '%s': "
                "scheme '%s' offset %u/%u ssp '%s' offset %u/%u",
                what, eid->c_str(),
                eid->scheme_str().c_str(),
                scheme_offset, dictionary_len,
                eid->ssp().c_str(),
                ssp_offset, dictionary_len);
        return false;                                                      
    }                                                                   
    
    log_debug(log, "parsed %s eid (offsets %u, %u) %s", 
	      what, scheme_offset, ssp_offset, eid->c_str());
    return true;
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::debug_dump_dictionary(const char* bp, size_t len,
                                             PrimaryBlock2* primary2)
{
#ifndef NDEBUG
    oasys::StringBuffer dict_copy;

    const char* end = bp + len;
    ASSERT(end[-1] == '\0');
    
    while (bp != end) {
        dict_copy.appendf("%s ", bp);
        bp += strlen(bp) + 1;
    }

    log_debug("/dtn/bundle/protocol",
              "dictionary len %zu, value: '%s'", len, dict_copy.c_str());
                  
    log_debug("/dtn/bundle/protocol",
              "dictionary offsets: dest %u,%u source %u,%u, "
              "custodian %u,%u replyto %u,%u",
              ntohs(primary2->dest_scheme_offset),
              ntohs(primary2->dest_ssp_offset),
              ntohs(primary2->source_scheme_offset),
              ntohs(primary2->source_ssp_offset),
              ntohs(primary2->custodian_scheme_offset),
              ntohs(primary2->custodian_ssp_offset),
              ntohs(primary2->replyto_scheme_offset),
              ntohs(primary2->replyto_ssp_offset));
#else
    (void)bp;
    (void)len;
    (void)primary2;
#endif
}

//----------------------------------------------------------------------
u_int8_t
PrimaryBlockProcessor::format_bundle_flags(const Bundle* bundle)
{
    u_int8_t flags = 0;

    if (bundle->is_fragment_) {
	flags |= BUNDLE_IS_FRAGMENT;
    }

    if (bundle->is_admin_) {
	flags |= BUNDLE_IS_ADMIN;
    }

    if (bundle->do_not_fragment_) {
	flags |= BUNDLE_DO_NOT_FRAGMENT;
    }

    if (bundle->custody_requested_) {
        flags |= BUNDLE_CUSTODY_XFER_REQUESTED;
    }

    if (bundle->singleton_dest_) {
        flags |= BUNDLE_SINGLETON_DESTINATION;
    }

    return flags;
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::parse_bundle_flags(Bundle* bundle, u_int8_t flags)
{
    if (flags & BUNDLE_IS_FRAGMENT) {
	bundle->is_fragment_ = true;
    }

    if (flags & BUNDLE_IS_ADMIN) {
	bundle->is_admin_ = true;
    }

    if (flags & BUNDLE_DO_NOT_FRAGMENT) {
	bundle->do_not_fragment_ = true;
    }

    if (flags & BUNDLE_CUSTODY_XFER_REQUESTED) {
	bundle->custody_requested_ = true;
    }

    if (flags & BUNDLE_SINGLETON_DESTINATION) {
	bundle->singleton_dest_ = true;
    }
}

//----------------------------------------------------------------------
u_int8_t
PrimaryBlockProcessor::format_cos_flags(const Bundle* b)
{
    u_int8_t cos_flags = 0;

    cos_flags = ((b->priority_ & 0x3) << 6);

    return cos_flags;
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::parse_cos_flags(Bundle* b, u_int8_t cos_flags)
{
    b->priority_ = ((cos_flags >> 6) & 0x3);
}

//----------------------------------------------------------------------
u_int8_t
PrimaryBlockProcessor::format_srr_flags(const Bundle* b)
{
    u_int8_t srr_flags = 0;
    
    if (b->receive_rcpt_)
	srr_flags |= BundleProtocol::STATUS_RECEIVED;

    if (b->custody_rcpt_)
	srr_flags |= BundleProtocol::STATUS_CUSTODY_ACCEPTED;

    if (b->forward_rcpt_)
	srr_flags |= BundleProtocol::STATUS_FORWARDED;

    if (b->delivery_rcpt_)
	srr_flags |= BundleProtocol::STATUS_DELIVERED;

    if (b->deletion_rcpt_)
	srr_flags |= BundleProtocol::STATUS_DELETED;

    if (b->app_acked_rcpt_)
        srr_flags |= BundleProtocol::STATUS_ACKED_BY_APP;

    return srr_flags;
}
    
//----------------------------------------------------------------------
void
PrimaryBlockProcessor::parse_srr_flags(Bundle* b, u_int8_t srr_flags)
{
    if (srr_flags & BundleProtocol::STATUS_RECEIVED)
	b->receive_rcpt_ = true;

    if (srr_flags & BundleProtocol::STATUS_CUSTODY_ACCEPTED)
	b->custody_rcpt_ = true;

    if (srr_flags & BundleProtocol::STATUS_FORWARDED)
	b->forward_rcpt_ = true;

    if (srr_flags & BundleProtocol::STATUS_DELIVERED)
	b->delivery_rcpt_ = true;

    if (srr_flags & BundleProtocol::STATUS_DELETED)
	b->deletion_rcpt_ = true;

    if (srr_flags & BundleProtocol::STATUS_ACKED_BY_APP)
	b->app_acked_rcpt_ = true;
}

//----------------------------------------------------------------------
size_t
PrimaryBlockProcessor::get_primary_len(const Bundle* bundle,
                                       DictionaryVector* dict,
                                       size_t* dictionary_len,
                                       size_t* primary_var_len)
{
    static const char* log = "/dtn/bundle/protocol";
    size_t primary_len = 0;
    *dictionary_len = 0;
    *primary_var_len = 0;
    
    /*
     * We need to figure out the total length of the primary block,
     * except for the SDNV used to encode the length itself and the
     * three byte preamble (PrimaryBlock1).
     *
     * First, we determine the size of the dictionary by first
     * figuring out all the unique strings, and in the process,
     * remembering their offsets and summing up their lengths
     * (including the null terminator for each).
     */
    add_to_dictionary(bundle->dest_,      dict, dictionary_len);
    add_to_dictionary(bundle->source_,    dict, dictionary_len);
    add_to_dictionary(bundle->custodian_, dict, dictionary_len);
    add_to_dictionary(bundle->replyto_,   dict, dictionary_len);

    (void)log; // in case NDEBUG is defined
    log_debug(log, "generated dictionary length %zu", *dictionary_len);

    *primary_var_len += SDNV::encoding_len(*dictionary_len);
    *primary_var_len += *dictionary_len;

    /*
     * If the bundle is a fragment, we need to include space for the
     * fragment offset and the original payload length.
     *
     * Note: Any changes to this protocol must be reflected into the
     * FragmentManager since it depends on this length when
     * calculating fragment sizes.
     */
    if (bundle->is_fragment_) {
	*primary_var_len += SDNV::encoding_len(bundle->frag_offset_);
	*primary_var_len += SDNV::encoding_len(bundle->orig_length_);
    }

    /*
     * Tack on the size of the PrimaryBlock2, 
     */
    *primary_var_len += sizeof(PrimaryBlock2);

    /*
     * Finally, add up the initial PrimaryBlock1 plus the size of the
     * SDNV to encode the variable length part, plus the variable
     * length part itself.
     */
    primary_len = sizeof(PrimaryBlock1) +
                  SDNV::encoding_len(*primary_var_len) +
                  *primary_var_len;
    
    log_debug(log, "get_primary_len(bundle %d): %zu",
              bundle->bundleid_, primary_len);
    
    return primary_len;
}

//----------------------------------------------------------------------
size_t
PrimaryBlockProcessor::get_primary_len(const Bundle* bundle)
{
    DictionaryVector dict;
    size_t dictionary_len;
    size_t primary_var_len;

    return get_primary_len(bundle, &dict, &dictionary_len, &primary_var_len);
}

//----------------------------------------------------------------------
int
PrimaryBlockProcessor::consume(Bundle* bundle, BlockInfo* block, u_char* buf, size_t len)
{
    static const char* log = "/dtn/bundle/protocol";
    size_t consumed = 0;
    
    ASSERT(! block->complete());
    
    /*
     * The first thing we need to do is find the length of the primary
     * block, noting that it may be split across multiple calls. 
     */
    if (block->data_length() == 0) {
        int cc = consume_preamble(block, buf, len, sizeof(PrimaryBlock1));
        
        if (cc == -1) {
            return -1; // protocol error
        }

        // If the data offset or data length aren't set, then the
        // whole buffer must have been consumed since there's not
        // enough data to parse the length.
        if (block->data_offset() == 0 || block->data_length() == 0) {
            ASSERT(cc == (int)len);
            return cc;
        }

        // Ok, we now know the whole length
        log_debug(log, "parsed primary block length %u (preamble %u)",
                  block->data_length(), block->data_offset());
        buf += cc;
        len -= cc;

        consumed += cc;
    }

    /*
     * Now see if this completes the primary by calling into
     * BlockProcessor::consume() to get the default behavior (now that
     * we've found the data length above).
     */
    ASSERT(block->full_length() > block->contents().len());
    int cc = BlockProcessor::consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return len;
    }

    /*
     * Ok, now the block is complete so we can parse it. We'll return
     * the total amount consumed (or -1 for protocol error) when
     * finished.
     */
    consumed += cc;

    size_t primary_len = block->full_length();

    buf = block->writable_contents()->buf();
    len = block->writable_contents()->len();

    ASSERT(primary_len == len);

    /*
     * Process the PrimaryBlock1
     */
    PrimaryBlock1* primary1 = (PrimaryBlock1*)buf;
    log_debug(log, "parsed primary block 1: version %d length %u",
              primary1->version, block->data_length());
    
    if (primary1->version != BundleProtocol::CURRENT_VERSION) {
	log_warn(log, "protocol version mismatch %d != %d",
		 primary1->version, BundleProtocol::CURRENT_VERSION);
	return -1;
    }
    
    parse_bundle_flags(bundle, primary1->bundle_processing_flags);
    parse_cos_flags(bundle, primary1->class_of_service_flags);
    parse_srr_flags(bundle, primary1->status_report_request_flags);

    /*
     * Now skip past the PrimaryBlock1 and the SDNV that describes the
     * total primary block length.
     */
    buf += block->data_offset();
    len -= block->data_offset();

    ASSERT(len == block->data_length());

    /*
     * Make sure that the sender didn't lie and that we really do have
     * enough for the next set of fixed-length fields. We'll return to
     * this label periodically as we parse different components.
     */
    u_int32_t primary2_len    = sizeof(PrimaryBlock2);
    int       dict_sdnv_len   = 0;
    u_int32_t dictionary_len  = 0;
    int       frag_offset_len = 0;
    int       frag_length_len = 0;
    if (len < sizeof(PrimaryBlock2)) {
tooshort:
	log_err(log, "primary block advertised incorrect length %u: "
		"fixed-length %u, dict_sdnv %d, dict %u, frag %d %d",
                block->data_length(), primary2_len, dict_sdnv_len,
                dictionary_len, frag_offset_len, frag_length_len);
	return -1;
    }

    /*
     * Parse the PrimaryBlock2
     */
    PrimaryBlock2* primary2 = (PrimaryBlock2*)buf;
    buf += sizeof(PrimaryBlock2);
    len -= sizeof(PrimaryBlock2);
    
    BundleProtocol::get_timestamp(&bundle->creation_ts_, &primary2->creation_ts);
    u_int32_t lifetime;
    memcpy(&lifetime, &primary2->lifetime, sizeof(lifetime));
    bundle->expiration_ = ntohl(lifetime);

    /*
     * Read the dictionary length.
     */
    dict_sdnv_len = SDNV::decode(buf, len, &dictionary_len);
    if (dict_sdnv_len < 0) {
        goto tooshort;
    }
    buf += dict_sdnv_len;
    len -= dict_sdnv_len;

    /*
     * Verify that we have the whole dictionary.
     */
    if (len < dictionary_len) {
        goto tooshort;
    }

    /*
     * Make sure that the dictionary ends with a null byte.
     */
    if (buf[dictionary_len - 1] != '\0') {
	log_err(log, "dictionary does not end with a NULL character!");
	return -1;
    }

    /*
     * Now use the dictionary buffer to parse out the various endpoint
     * identifiers, making sure that none of them peeks past the end
     * of the dictionary block.
     */
    u_char* dictionary = buf;
    buf += dictionary_len;
    len -= dictionary_len;

    debug_dump_dictionary((char*)dictionary, dictionary_len, primary2);

    extract_dictionary_eid(&bundle->source_, "source",
                           &primary2->source_scheme_offset,
                           &primary2->source_ssp_offset,
                           dictionary, dictionary_len);
    
    extract_dictionary_eid(&bundle->dest_, "dest",
                           &primary2->dest_scheme_offset,
                           &primary2->dest_ssp_offset,
                           dictionary, dictionary_len);
    
    extract_dictionary_eid(&bundle->replyto_, "replyto",
                           &primary2->replyto_scheme_offset,
                           &primary2->replyto_ssp_offset,
                           dictionary, dictionary_len);
    
    extract_dictionary_eid(&bundle->custodian_, "custodian",
                           &primary2->custodian_scheme_offset,
                           &primary2->custodian_ssp_offset,
                           dictionary, dictionary_len);
    
    if (bundle->is_fragment_) {
	frag_offset_len = SDNV::decode(buf, len, &bundle->frag_offset_);
	if (frag_offset_len == -1) {
	    goto tooshort;
	}
	buf += frag_offset_len;
	len -= frag_offset_len;

	frag_length_len = SDNV::decode(buf, len, &bundle->orig_length_);
	if (frag_length_len == -1) {
	    goto tooshort;
	}
	buf += frag_length_len;
	len -= frag_length_len;

	log_debug(log, "parsed fragmentation info: offset %u, orig_len %u",
		  bundle->frag_offset_, bundle->orig_length_);
    }

    return consumed;
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::generate(const Bundle* bundle,
                                Link*         link,
                                BlockInfo*    block,
                                bool          last)
{
    (void)link;

    ASSERT(!last); // primary can't be last
    
    static const char* log = "/dtn/bundle/protocol";
    DictionaryVector dict;
    size_t primary_len = 0;     // total length of the primary block
    size_t primary_var_len = 0; // length of the variable part of the primary
    size_t dictionary_len = 0;  // length of the dictionary
    int sdnv_len = 0;		// use an int to handle -1 return values

    /*
     * Calculate the primary block length and initialize the buffer.
     */
    primary_len = get_primary_len(bundle, &dict, &dictionary_len,
                                  &primary_var_len);
    block->writable_contents()->reserve(primary_len);
    block->writable_contents()->set_len(primary_len);
    block->set_data_length(primary_len);

    /*
     * Advance buf and decrement len as we go through the process.
     */
    u_char* buf = block->writable_contents()->buf();
    int     len = primary_len;
    
    (void)log; // in case NDEBUG is defined
    log_debug(log, "generating primary: length %zu (preamble %zu var length %zu)",
              primary_len,
	      (sizeof(PrimaryBlock1) + SDNV::encoding_len(primary_var_len)),
	      primary_var_len);
    
    /*
     * Ok, stuff in the preamble and the total block length.
     */
    PrimaryBlock1* primary1               = (PrimaryBlock1*)buf;
    primary1->version		          = BundleProtocol::CURRENT_VERSION;
    primary1->bundle_processing_flags     = format_bundle_flags(bundle);
    primary1->class_of_service_flags	  = format_cos_flags(bundle);
    primary1->status_report_request_flags = format_srr_flags(bundle);
    
    sdnv_len = SDNV::encode(primary_var_len,
                            &primary1->block_length[0], len - 3);
    ASSERT(sdnv_len > 0);
    buf += (sizeof(PrimaryBlock1) + sdnv_len);
    len -= (sizeof(PrimaryBlock1) + sdnv_len);

    /*
     * Now fill in the PrimaryBlock2.
     */
    PrimaryBlock2* primary2 = (PrimaryBlock2*)buf;

    get_dictionary_offsets(&dict, bundle->dest_,
                           &primary2->dest_scheme_offset,
                           &primary2->dest_ssp_offset);

    get_dictionary_offsets(&dict, bundle->source_,
                           &primary2->source_scheme_offset,
                           &primary2->source_ssp_offset);

    get_dictionary_offsets(&dict, bundle->custodian_,
                           &primary2->custodian_scheme_offset,
                           &primary2->custodian_ssp_offset);

    get_dictionary_offsets(&dict, bundle->replyto_,
                           &primary2->replyto_scheme_offset,
                           &primary2->replyto_ssp_offset);

    BundleProtocol::set_timestamp(&primary2->creation_ts, &bundle->creation_ts_);
    u_int32_t lifetime = htonl(bundle->expiration_);
    memcpy(&primary2->lifetime, &lifetime, sizeof(lifetime));

    buf += sizeof(PrimaryBlock2);
    len -= sizeof(PrimaryBlock2);

    /*
     * Stuff in the dictionary length and dictionary bytes.
     */
    sdnv_len = SDNV::encode(dictionary_len, buf, len);
    ASSERT(sdnv_len > 0);
    buf += sdnv_len;
    len -= sdnv_len;

    DictionaryVector::iterator dict_iter;
    for (dict_iter = dict.begin(); dict_iter != dict.end(); ++dict_iter) {
	strcpy((char*)buf, dict_iter->str.c_str());
	buf += dict_iter->str.length() + 1;
	len -= dict_iter->str.length() + 1;
    }
    
    debug_dump_dictionary((char*)buf - dictionary_len, dictionary_len, primary2);
    
    /*
     * If the bundle is a fragment, stuff in SDNVs for the fragment
     * offset and original length.
     */
    if (bundle->is_fragment_) {
	sdnv_len = SDNV::encode(bundle->frag_offset_, buf, len);
	ASSERT(sdnv_len > 0);
	buf += sdnv_len;
	len -= sdnv_len;

	sdnv_len = SDNV::encode(bundle->orig_length_, buf, len);
	ASSERT(sdnv_len > 0);
	buf += sdnv_len;
	len -= sdnv_len;
    }

#ifndef NDEBUG
    {
        DictionaryVector dict2;
        size_t dict2_len = 0;
        size_t p2len;
        size_t len2 = get_primary_len(bundle, &dict2, &dict2_len, &p2len);
        ASSERT(len2 == primary_len);
    }
#endif

    /*
     * Asuming that get_primary_len is written correctly, len should
     * now be zero since we initialized it to primary_len at the
     * beginning of the function.
     */
    ASSERT(len == 0);
}


} // namespace dtn

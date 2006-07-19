/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 *
 * Intel Open Source License
 *
 * Copyright (c) 2004 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <algorithm>

#include <oasys/debug/DebugUtils.h>
#include <oasys/util/StringUtils.h>

#include "Bundle.h"
#include "BundleProtocol.h"
#include "BundleTimestamp.h"
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
void
BundleProtocol::add_to_dictionary(const EndpointID& eid,
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
BundleProtocol::get_dictionary_offsets(DictionaryVector *dict,
                                       EndpointID eid,
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
size_t
BundleProtocol::get_primary_len(const Bundle* bundle,
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
BundleProtocol::get_payload_block_len(const Bundle* bundle)
{
    return (sizeof(BlockPreamble) +
            SDNV::encoding_len(bundle->payload_.length()));
}

//----------------------------------------------------------------------
int
BundleProtocol::format_header_blocks(const Bundle* bundle,
                                     u_char* buf, size_t len)
{
    static const char* log = "/dtn/bundle/protocol";
    DictionaryVector dict;
    size_t orig_len = len;      // original length of the buffer
    size_t primary_len = 0;     // total length of the primary block
    size_t primary_var_len = 0; // length of the variable part of the primary
    size_t dictionary_len = 0;  // length of the dictionary
    int encoding_len = 0;	// use an int to handle -1 return values

    /*
     * Grab the primary block length and make sure we have enough
     * space in the buffer for it.
     */
    primary_len = get_primary_len(bundle, &dict, &dictionary_len,
                                  &primary_var_len);
    if (len < primary_len) {
	return -1;
    }
    
    (void)log; // in case NDEBUG is defined
    log_debug(log, "primary length %zu (preamble %zu var length %zu)",
              primary_len,
	      (sizeof(PrimaryBlock1) + SDNV::encoding_len(primary_var_len)),
	      primary_var_len);
    
    /*
     * Ok, stuff in the preamble and the total block length.
     */
    PrimaryBlock1* primary1              = (PrimaryBlock1*)buf;
    primary1->version		          = CURRENT_VERSION;
    primary1->bundle_processing_flags     = format_bundle_flags(bundle);
    primary1->class_of_service_flags	  = format_cos_flags(bundle);
    primary1->status_report_request_flags = format_srr_flags(bundle);
    
    encoding_len = SDNV::encode(primary_var_len,
				&primary1->block_length[0], len - 3);
    ASSERT(encoding_len > 0);
    buf += (sizeof(PrimaryBlock1) + encoding_len);
    len -= (sizeof(PrimaryBlock1) + encoding_len);

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

    log_debug(log, "dictionary offsets: dest %u,%u source %u,%u, "
	      "custodian %u,%u replyto %u,%u",
	      ntohs(primary2->dest_scheme_offset),
	      ntohs(primary2->dest_ssp_offset),
	      ntohs(primary2->source_scheme_offset),
	      ntohs(primary2->source_ssp_offset),
	      ntohs(primary2->custodian_scheme_offset),
	      ntohs(primary2->custodian_ssp_offset),
	      ntohs(primary2->replyto_scheme_offset),
	      ntohs(primary2->replyto_ssp_offset));

    set_timestamp(&primary2->creation_ts, &bundle->creation_ts_);
    u_int32_t lifetime = htonl(bundle->expiration_);
    memcpy(&primary2->lifetime, &lifetime, sizeof(lifetime));

    buf += sizeof(PrimaryBlock2);
    len -= sizeof(PrimaryBlock2);

    /*
     * Stuff in the dictionary length and dictionary bytes.
     */
    encoding_len = SDNV::encode(dictionary_len, buf, len);
    ASSERT(encoding_len > 0);
    buf += encoding_len;
    len -= encoding_len;

    DictionaryVector::iterator dict_iter;
    for (dict_iter = dict.begin(); dict_iter != dict.end(); ++dict_iter) {
	strcpy((char*)buf, dict_iter->str.c_str());
	buf += dict_iter->str.length() + 1;
	len -= dict_iter->str.length() + 1;
    }

    /*
     * If the bundle is a fragment, stuff in SDNVs for the fragment
     * offset and original length.
     */
    if (bundle->is_fragment_) {
	encoding_len = SDNV::encode(bundle->frag_offset_, buf, len);
	ASSERT(encoding_len > 0);
	buf += encoding_len;
	len -= encoding_len;

	encoding_len = SDNV::encode(bundle->orig_length_, buf, len);
	ASSERT(encoding_len > 0);
	buf += encoding_len;
	len -= encoding_len;
    }

#ifndef NDEBUG
    {
        DictionaryVector dict2;
        size_t dict2_len = 0;
        size_t p2len;
        size_t len2 = get_primary_len(bundle, &dict2, &dict2_len, &p2len);
        ASSERT(len2 == (orig_len - len));
    }
#endif

    // safety assertion
    ASSERT(primary_len == (orig_len - len));

    // XXX/demmer deal with other experimental blocks

    /*
     * Handle the payload block. Note that we don't stuff in the
     * actual payload here, even though it's technically part of the
     * "payload block".
     */
    u_int32_t payload_len = bundle->payload_.length();
    if (len < (sizeof(BlockPreamble) +
	       SDNV::encoding_len(payload_len)))
    {
	return -1;
    }

    BlockPreamble* payload_block = (BlockPreamble*)buf;
    payload_block->type  = PAYLOAD_BLOCK;
    payload_block->flags = BLOCK_FLAG_LAST_BLOCK;
    buf += sizeof(BlockPreamble);
    len -= sizeof(BlockPreamble);

    encoding_len = SDNV::encode(payload_len, buf, len);
    ASSERT(encoding_len > 0);
    buf += encoding_len;
    len -= encoding_len;

    // return the total buffer length consumed
    log_debug(log, "encoding done -- total length %zu", (orig_len - len));
    return orig_len - len;
}

//----------------------------------------------------------------------
int
BundleProtocol::parse_header_blocks(Bundle* bundle,
                                    u_char* buf, size_t len)
{
    static const char* log = "/dtn/bundle/protocol";
    size_t origlen = len;
    int encoding_len;

    /*
     * First the three bytes of the PrimaryBlock1
     */
    PrimaryBlock1* primary1;
    if (len < sizeof(PrimaryBlock1)) {
 tooshort1:
	log_debug(log, "buffer too short (parsed %zu/%zu)",
		  (origlen - len), origlen);
	return -1;
    }

    primary1 = (PrimaryBlock1*)buf;
    buf += sizeof(PrimaryBlock1);
    len -= sizeof(PrimaryBlock1);

    log_debug(log, "parsed primary block 1: version %d", primary1->version);

    if (primary1->version != CURRENT_VERSION) {
	log_warn(log, "protocol version mismatch %d != %d",
		 primary1->version, CURRENT_VERSION);
	return -1;
    }

    parse_bundle_flags(bundle, primary1->bundle_processing_flags);
    parse_cos_flags(bundle, primary1->class_of_service_flags);
    parse_srr_flags(bundle, primary1->status_report_request_flags);

    /*
     * Now parse the SDNV that describes the total primary block
     * length.
     */
    u_int32_t primary_len;
    encoding_len = SDNV::decode(buf, len, &primary_len);
    if (encoding_len == -1) {
	goto tooshort1;
    }

    buf += encoding_len;
    len -= encoding_len;

    log_debug(log, "parsed primary block length %u", primary_len);

    /*
     * Check if the advertised length is bigger than the amount we
     * have.
     */
    if (len < primary_len) {
	goto tooshort1;
    }

    /*
     * Still, we need to make sure that the sender didn't lie and that
     * we really do have enough for the decoding...
     */
    if (len < sizeof(PrimaryBlock2)) {
 tooshort2:
	log_err(log, "primary block advertised incorrect length: "
		"advertised %u, total buffer %zu", primary_len, len);
	return -1;
    }

    /*
     * Parse the PrimaryBlock2
     */
    PrimaryBlock2* primary2    = (PrimaryBlock2*)buf;
    buf += sizeof(PrimaryBlock2);
    len -= sizeof(PrimaryBlock2);

    get_timestamp(&bundle->creation_ts_, &primary2->creation_ts);
    u_int32_t lifetime;
    memcpy(&lifetime, &primary2->lifetime, sizeof(lifetime));
    bundle->expiration_ = ntohl(lifetime);

    /*
     * Read the dictionary length.
     */
    u_int32_t dictionary_len = 0;
    encoding_len = SDNV::decode(buf, len, &dictionary_len);
    if (encoding_len < 0) {
	goto tooshort2;
    }
    buf += encoding_len;
    len -= encoding_len;

    /*
     * Verify that we have the whole dictionary.
     */
    if (len < dictionary_len) {
	goto tooshort2;
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

    u_int16_t scheme_offset, ssp_offset;

#define EXTRACT_DICTIONARY_EID(_what)                                   \
    memcpy(&scheme_offset, &primary2->_what##_scheme_offset, 2);        \
    memcpy(&ssp_offset, &primary2->_what##_ssp_offset, 2);              \
    scheme_offset = ntohs(scheme_offset);                               \
    ssp_offset = ntohs(ssp_offset);                                     \
									\
    if (scheme_offset > (dictionary_len - 1)) {                         \
	log_err(log, "illegal offset for %s scheme dictionary offset: " \
		"offset %d, total length %u", #_what,                   \
		scheme_offset, dictionary_len);                         \
	return -1;                                                      \
    }                                                                   \
									\
    if (ssp_offset > (dictionary_len - 1)) {                            \
	log_err(log, "illegal offset for %s ssp dictionary offset: "    \
		"offset %d, total length %u", #_what,                   \
		ssp_offset, dictionary_len);                            \
	return -1;                                                      \
    }                                                                   \
    bundle->_what##_.assign((char*)&dictionary[scheme_offset],          \
			    (char*)&dictionary[ssp_offset]);            \
									\
									\
    if (! bundle->_what##_.valid()) {                                   \
	log_err(log, "invalid %s endpoint id '%s'", #_what,             \
		bundle->_what##_.c_str());                              \
	return -1;                                                      \
    }                                                                   \
									\
    log_debug(log, "parsed %s eid (offsets %d, %d) %s", #_what,         \
	      scheme_offset, ssp_offset, bundle->_what##_.c_str());     \

    EXTRACT_DICTIONARY_EID(source);
    EXTRACT_DICTIONARY_EID(dest);
    EXTRACT_DICTIONARY_EID(custodian);
    EXTRACT_DICTIONARY_EID(replyto);

    if (bundle->is_fragment_) {
	encoding_len = SDNV::decode(buf, len, &bundle->frag_offset_);
	if (encoding_len == -1) {
	    goto tooshort2;
	}
	buf += encoding_len;
	len -= encoding_len;

	encoding_len = SDNV::decode(buf, len, &bundle->orig_length_);
	if (encoding_len == -1) {
	    goto tooshort2;
	}
	buf += encoding_len;
	len -= encoding_len;

	log_debug(log, "parsed fragmentation info: offset %u, orig_len %u",
		  bundle->frag_offset_, bundle->orig_length_);
    }

    /*
     * Parse the other blocks, all of which have a preamble and an
     * SDNV for their length.
     */
    while (len != 0) {
	if (len <= sizeof(BlockPreamble)) {
	    goto tooshort1;
	}

	BlockPreamble* preamble = (BlockPreamble*)buf;
	buf += sizeof(BlockPreamble);
	len -= sizeof(BlockPreamble);

	// note that we don't support lengths bigger than 4 bytes
	// here. this should be fixed when the bundle payload class
	// supports big payloads.
	u_int32_t block_len;
	encoding_len = SDNV::decode(buf, len, &block_len);
	if (encoding_len == -1) {
	    goto tooshort1;
	}
	buf += encoding_len;
	len -= encoding_len;

	switch (preamble->type) {
	case PAYLOAD_BLOCK: {
	    bundle->payload_.set_length(block_len);
	    log_debug(log, "parsed payload length %zu",
		      bundle->payload_.length());

            // XXX/demmer add support for blocks after the payload block
            if (! (preamble->flags & BLOCK_FLAG_LAST_BLOCK)) {
                log_crit(log,
                         "this implementation cannot handle blocks after "
                         "the payload!!");
            }
            
	    // note that we don't actually snarf in the payload here
	    // since it's handled by the caller, and since the payload
	    // must be the last thing we handle in this function,
	    // we're done.
	    goto done;
	}
            
	default:
	    // XXX/demmer handle extension blocks.
	    log_err(log, "unknown block code 0x%x", preamble->type);
	    return -1;
	}
    }

    // that's all we parse, return the amount we consumed
 done:
    return origlen - len;
}

//----------------------------------------------------------------------
size_t
BundleProtocol::formatted_length(const Bundle* bundle)
{
    DictionaryVector dict;
    size_t dictionary_len;
    size_t primary_var_len;

    // XXX/demmer if this ends up being slow, we can cache it in the bundle
    // XXX/demmer need to add tail blocks as well
    
    return (get_primary_len(bundle, &dict, &dictionary_len, &primary_var_len) +
            get_payload_block_len(bundle) +
            bundle->payload_.length());
}
    
//----------------------------------------------------------------------
void
BundleProtocol::set_timestamp(u_char* ts, const BundleTimestamp* tv)
{
    u_int32_t tmp;

    tmp = htonl(tv->seconds_);
    memcpy(ts, &tmp, sizeof(u_int32_t));
    ts += sizeof(u_int32_t);

    tmp = htonl(tv->seqno_);
    memcpy(ts, &tmp, sizeof(u_int32_t));
}

//----------------------------------------------------------------------
void
BundleProtocol::get_timestamp(BundleTimestamp* tv, const u_char* ts)
{
    u_int32_t tmp;

    memcpy(&tmp, ts, sizeof(u_int32_t));
    tv->seconds_  = ntohl(tmp);
    ts += sizeof(u_int32_t);
    
    memcpy(&tmp, ts, sizeof(u_int32_t));
    tv->seqno_ = ntohl(tmp);
}

//----------------------------------------------------------------------
u_int8_t
BundleProtocol::format_bundle_flags(const Bundle* bundle)
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
BundleProtocol::parse_bundle_flags(Bundle* bundle, u_int8_t flags)
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
BundleProtocol::format_cos_flags(const Bundle* b)
{
    u_int8_t cos_flags = 0;

    cos_flags = ((b->priority_ & 0x3) << 6);

    return cos_flags;
}

//----------------------------------------------------------------------
void
BundleProtocol::parse_cos_flags(Bundle* b, u_int8_t cos_flags)
{
    b->priority_ = ((cos_flags >> 6) & 0x3);
}

//----------------------------------------------------------------------
u_int8_t
BundleProtocol::format_srr_flags(const Bundle* b)
{
    u_int8_t srr_flags = 0;
    
    if (b->receive_rcpt_)
	srr_flags |= STATUS_RECEIVED;

    if (b->custody_rcpt_)
	srr_flags |= STATUS_CUSTODY_ACCEPTED;

    if (b->forward_rcpt_)
	srr_flags |= STATUS_FORWARDED;

    if (b->delivery_rcpt_)
	srr_flags |= STATUS_DELIVERED;

    if (b->deletion_rcpt_)
	srr_flags |= STATUS_DELETED;

    if (b->app_acked_rcpt_)
        srr_flags |= STATUS_ACKED_BY_APP;

    return srr_flags;
}
    
//----------------------------------------------------------------------
void
BundleProtocol::parse_srr_flags(Bundle* b, u_int8_t srr_flags)
{
    if (srr_flags & STATUS_RECEIVED)
	b->receive_rcpt_ = true;

    if (srr_flags & STATUS_CUSTODY_ACCEPTED)
	b->custody_rcpt_ = true;

    if (srr_flags & STATUS_FORWARDED)
	b->forward_rcpt_ = true;

    if (srr_flags & STATUS_DELIVERED)
	b->delivery_rcpt_ = true;

    if (srr_flags & STATUS_DELETED)
	b->deletion_rcpt_ = true;

    if (srr_flags & STATUS_ACKED_BY_APP)
	b->app_acked_rcpt_ = true;
}

//----------------------------------------------------------------------
bool
BundleProtocol::get_admin_type(const Bundle* bundle, admin_record_type_t* type)
{
    if (! bundle->is_admin_) {
	return false;
    }

    u_char buf[16];
    const u_char* bp = bundle->payload_.read_data(0, sizeof(buf), buf);

    switch (bp[0] >> 4)
    {
#define CASE(_what) case _what: *type = _what; return true;

	CASE(ADMIN_STATUS_REPORT);
	CASE(ADMIN_CUSTODY_SIGNAL);
	CASE(ADMIN_ECHO);
	CASE(ADMIN_NULL);
	CASE(ADMIN_ANNOUNCE);

#undef  CASE
    default:
        return false; // unknown type
    }

    NOTREACHED;
}

} // namespace dtn

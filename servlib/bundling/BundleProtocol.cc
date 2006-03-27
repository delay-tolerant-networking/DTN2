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
#include "SDNV.h"

namespace dtn {

struct DictionaryEntry {
    DictionaryEntry(const std::string& s, size_t off)
	: str(s), offset(off) {}

    std::string str;
    size_t offset;
};

typedef std::vector<DictionaryEntry> DictionaryVector;

/**
 * For the scheme and ssp parts of the given endpoint id, see if
 * they've already appeared in the vector. If not, add them, and
 * record their length (with the null terminator) in the running
 * length total.
 */
static inline void
add_to_dict(const EndpointID& eid, DictionaryVector* dict, size_t* dictlen)
{
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

void
get_dict_offsets(DictionaryVector *dict, EndpointID eid,
		 u_int16_t* scheme_offset, u_int16_t* ssp_offset)
{
    DictionaryVector::iterator iter;
    for (iter = dict->begin(); iter != dict->end(); ++iter) {
	if (iter->str == eid.scheme_str())
	    *scheme_offset = htons(iter->offset);

	if (iter->str == eid.ssp())
	    *ssp_offset = htons(iter->offset);
    }
}

/**
 * Fill in the given buffer with the formatted bundle headers
 * including the payload header, but not the payload data.
 *
 * @return the total length of the header or -1 on error
 */
int
BundleProtocol::format_headers(const Bundle* bundle, u_char* buf, size_t len)
{
    static const char* log = "/dtn/bundle/protocol";
    DictionaryVector dict;
    size_t orig_len = len;
    size_t primary_len = 0;
    size_t dictionary_len = 0;
    int encoding_len = 0;	// use an int to handle -1 return values

    /*
     * We need to figure out the total length of the primary header,
     * except for the SDNV used to encode the length itself and the
     * three byte preamble (PrimaryHeader1).
     *
     * First, we determine the size of the dictionary by first
     * figuring out all the unique strings, and in the process,
     * remembering their offsets and summing up their lengths
     * (including the null terminator for each).
     */
    add_to_dict(bundle->dest_,      &dict, &dictionary_len);
    add_to_dict(bundle->source_,    &dict, &dictionary_len);
    add_to_dict(bundle->custodian_, &dict, &dictionary_len);
    add_to_dict(bundle->replyto_,   &dict, &dictionary_len);

    log_debug(log, "generated dictionary length %u", (u_int)dictionary_len);

    primary_len += SDNV::encoding_len(dictionary_len);
    primary_len += dictionary_len;

    /*
     * If the bundle is a fragment, we need to include space for the
     * fragment offset and the original payload length.
     */
    if (bundle->is_fragment_) {
	primary_len += SDNV::encoding_len(bundle->frag_offset_);
	primary_len += SDNV::encoding_len(bundle->orig_length_);
    }

    /*
     * Finally, tack on the PrimaryHeader2.
     */
    primary_len += sizeof(PrimaryHeader2);

    /*
     * Now, make sure we have enough space in the buffer for the whole
     * primary header.
     */
    if (len < (sizeof(PrimaryHeader1) +
	       SDNV::encoding_len(primary_len) +
	       primary_len))
    {
	return -1;
    }

    log_debug(log, "preamble length %u, primary length %u",
	      (u_int)(sizeof(PrimaryHeader1) + SDNV::encoding_len(primary_len)),
	      (u_int)primary_len);

    /*
     * Ok, stuff in the preamble and the total header length.
     */
    PrimaryHeader1* primary1              = (PrimaryHeader1*)buf;
    primary1->version		          = CURRENT_VERSION;
    primary1->bundle_processing_flags     = format_bundle_flags(bundle);
    primary1->class_of_service_flags	  = format_cos_flags(bundle);
    primary1->status_report_request_flags = format_srr_flags(bundle);

    encoding_len = SDNV::encode(primary_len,
				&primary1->header_length[0], len - 3);
    ASSERT(encoding_len > 0);
    buf += (sizeof(PrimaryHeader1) + encoding_len);
    len -= (sizeof(PrimaryHeader1) + encoding_len);

    /*
     * Now fill in the PrimaryHeader2.
     */
    PrimaryHeader2* primary2    = (PrimaryHeader2*)buf;

    get_dict_offsets(&dict, bundle->dest_,
		     &primary2->dest_scheme_offset,
		     &primary2->dest_ssp_offset);

    get_dict_offsets(&dict, bundle->source_,
		     &primary2->source_scheme_offset,
		     &primary2->source_ssp_offset);

    get_dict_offsets(&dict, bundle->custodian_,
		     &primary2->custodian_scheme_offset,
		     &primary2->custodian_ssp_offset);

    get_dict_offsets(&dict, bundle->replyto_,
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

    buf += sizeof(PrimaryHeader2);
    len -= sizeof(PrimaryHeader2);

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

    // XXX/demmer deal with other experimental headers

    /*
     * Payload header (must be last). Note that we don't stuff in the
     * actual payload here, even though it's technically part of the
     * "payload header".
     */
    u_int32_t payload_len = bundle->payload_.length();
    if (len < (sizeof(HeaderPreamble) +
	       SDNV::encoding_len(payload_len)))
    {
	return -1;
    }

    HeaderPreamble* payload_header = (HeaderPreamble*)buf;
    payload_header->type = HEADER_PAYLOAD;
    payload_header->flags = 0;
    buf += sizeof(HeaderPreamble);
    len -= sizeof(HeaderPreamble);

    encoding_len = SDNV::encode(payload_len, buf, len);
    ASSERT(encoding_len > 0);
    buf += encoding_len;
    len -= encoding_len;

    // return the total buffer length consumed
    log_debug(log, "encoding done -- total length %u", (u_int)(orig_len - len));
    return orig_len - len;
}

int
BundleProtocol::parse_headers(Bundle* bundle, u_char* buf, size_t len)
{
    static const char* log = "/dtn/bundle/protocol";
    size_t origlen = len;
    int encoding_len;

    /*
     * First the three bytes of the PrimaryHeader1
     */
    PrimaryHeader1* primary1;
    if (len < sizeof(PrimaryHeader1)) {
 tooshort1:
	log_debug(log, "buffer too short (parsed %u/%u)",
		  (u_int)(origlen - len), (u_int)origlen);
	return -1;
    }

    primary1 = (PrimaryHeader1*)buf;
    buf += sizeof(PrimaryHeader1);
    len -= sizeof(PrimaryHeader1);

    log_debug(log, "parsed primary header 1: version %d", primary1->version);

    if (primary1->version != CURRENT_VERSION) {
	log_warn(log, "protocol version mismatch %d != %d",
		 primary1->version, CURRENT_VERSION);
	return -1;
    }

    parse_bundle_flags(bundle, primary1->bundle_processing_flags);
    parse_cos_flags(bundle, primary1->class_of_service_flags);
    parse_srr_flags(bundle, primary1->status_report_request_flags);

    /*
     * Now parse the SDNV that describes the total primary header
     * length.
     */
    u_int32_t primary_len;
    encoding_len = SDNV::decode(buf, len, &primary_len);
    if (encoding_len == -1) {
	goto tooshort1;
    }

    buf += encoding_len;
    len -= encoding_len;

    log_debug(log, "parsed primary header length %u", primary_len);

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
    if (len < sizeof(PrimaryHeader2)) {
 tooshort2:
	log_err(log, "primary header advertised incorrect length: "
		"advertised %u, total buffer %d", primary_len, (u_int)len);
	return -1;
    }

    /*
     * Parse the PrimaryHeader2
     */
    PrimaryHeader2* primary2    = (PrimaryHeader2*)buf;
    buf += sizeof(PrimaryHeader2);
    len -= sizeof(PrimaryHeader2);

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
     * of the dictionary header.
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
     * Parse the other headers, all of which have a preamble and an
     * SDNV for their length.
     */
    while (len != 0) {
	if (len <= sizeof(HeaderPreamble)) {
	    goto tooshort1;
	}

	HeaderPreamble* preamble = (HeaderPreamble*)buf;
	buf += sizeof(HeaderPreamble);
	len -= sizeof(HeaderPreamble);

	// note that we don't support lengths bigger than 4 bytes
	// here. this should be fixed when the bundle payload class
	// supports big payloads.
	u_int32_t header_len;
	encoding_len = SDNV::decode(buf, len, &header_len);
	if (encoding_len == -1) {
	    goto tooshort1;
	}
	buf += encoding_len;
	len -= encoding_len;

	switch (preamble->type) {
	case HEADER_PAYLOAD: {
	    bundle->payload_.set_length(header_len);
	    log_debug(log, "parsed payload length %u",
		      (u_int)bundle->payload_.length());

	    // note that we don't actually snarf in the payload here
	    // since it's handled by the caller, and since the payload
	    // must be last, we're all done
	    goto done;
	}

	default:
	    // XXX/demmer handle extension headers.
	    log_err(log, "unknown header code 0x%x", preamble->type);
	    return -1;
	}
    }

    // that's all we parse, return the amount we consumed
 done:
    return origlen - len;
}


void
BundleProtocol::set_timestamp(u_char* ts, const struct timeval* tv)
{
    u_int64_t ts_tmp =
	(((u_int64_t)htonl(tv->tv_sec)) << 32) | htonl(tv->tv_usec);
    memcpy(ts, &ts_tmp, sizeof(u_int64_t));
}

void
BundleProtocol::get_timestamp(struct timeval* tv, const u_char* ts)
{
    u_int64_t ts_tmp;
    memcpy(&ts_tmp, ts, sizeof(ts_tmp));

    tv->tv_sec  = ntohl(ts_tmp >> 32);
    tv->tv_usec = ntohl((u_int32_t)ts_tmp);
}

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

u_int8_t
BundleProtocol::format_cos_flags(const Bundle* b)
{
    u_int8_t cos_flags = 0;

    cos_flags = ((b->priority_ & 0x3) << 6);

    return cos_flags;
}

void
BundleProtocol::parse_cos_flags(Bundle* b, u_int8_t cos_flags)
{
    b->priority_ = ((cos_flags >> 6) & 0x3);
}

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

#undef  CASE
    }

    return false; // unknown type
}


} // namespace dtn

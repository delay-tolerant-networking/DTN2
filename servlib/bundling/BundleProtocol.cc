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

/*
 * NTP timestamp conversion routines included from the ntp source
 * distribution and is therefore subject to the following copyright.
 * 
 ***********************************************************************
 *                                                                     *
 * Copyright (c) David L. Mills 1992-2003                              *
 *                                                                     *
 * Permission to use, copy, modify, and distribute this software and   *
 * its documentation for any purpose and without fee is hereby         *
 * granted, provided that the above copyright notice appears in all    *
 * copies and that both the copyright notice and this permission       *
 * notice appear in supporting documentation, and that the name        *
 * University of Delaware not be used in advertising or publicity      *
 * pertaining to distribution of the software without specific,        *
 * written prior permission. The University of Delaware makes no       *
 * representations about the suitability this software for any         *
 * purpose. It is provided "as is" without express or implied          *
 * warranty.                                                           *
 *                                                                     *
 ***********************************************************************
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <algorithm>

#include <oasys/debug/Debug.h>
#include <oasys/util/StringUtils.h>

#include "AddressFamily.h"
#include "Bundle.h"
#include "BundleProtocol.h"

namespace dtn {

/**
 * For the region and admin parts of the given tuple, fill in the
 * corresponding index into the string vector.
 */
static inline void
add_to_dict(u_int8_t* id, const BundleTuple& tuple,
            oasys::StringVector* tuples, size_t* dictlen)
{
    u_int8_t region_id, admin_id;
    oasys::StringVector::iterator iter;

    // first the region string
    iter = std::find(tuples->begin(), tuples->end(), tuple.region());
    if (iter == tuples->end()) {
        tuples->push_back(tuple.region());
        region_id = tuples->size() - 1;
        *dictlen += (tuple.region().length() + 1);
    } else {
        region_id = iter - tuples->begin();
    }

    // then the admin string
    iter = std::find(tuples->begin(), tuples->end(), tuple.admin());
    if (iter == tuples->end()) {
        tuples->push_back(tuple.admin());
        admin_id = tuples->size() - 1;
        *dictlen += (tuple.admin().length() + 1);
    } else {
        admin_id = iter - tuples->begin();
    }

    // cons up the id field, making sure each id fits in 4 bits
    ASSERT((region_id <= 0xf) && (admin_id <= 0xf));
    *id = ((region_id << 4) | admin_id);
}

/**
 * Fill in the given iovec with the formatted bundle header.
 *
 * @return the total length of the header or -1 on error
 */
size_t
BundleProtocol::format_headers(const Bundle* bundle,
                               struct iovec* iov, int* iovcnt)
{
    // make sure that iovcnt is big enough for:
    // 1) primary header
    // 2) dictionary header and string data
    // 3) payload header
    //
    // optional ones including:
    // 4) fragmentation header
    // 5) authentication header
    // 6) extension headers (someday)
    ASSERT(*iovcnt >= 6);

    // by definition, all headers have next_header_type as their first
    // byte, so this can be used to set the type field as we progress
    // through building the vector.
    u_int8_t* next_header_type;

    //
    // primary header
    //
    PrimaryHeader* primary      = (PrimaryHeader*)malloc(sizeof (PrimaryHeader));
    primary->version 		= CURRENT_VERSION;
    primary->class_of_service 	= format_cos(bundle);
    primary->payload_security 	= format_payload_security(bundle);
    primary->expiration 	= htonl(bundle->expiration_);
    set_timestamp(&primary->creation_ts, &bundle->creation_ts_);

    next_header_type = &primary->next_header_type;
    iov[0].iov_base = (char*)primary;
    iov[0].iov_len  = sizeof(PrimaryHeader);
    *iovcnt = 1;

    // now figure out how many unique tuples there are, storing the
    // unique ones in a temp vector and assigning the corresponding
    // string ids in the primary header. keep track of the total
    // length of the dictionary header in the process
    oasys::StringVector tuples;
    size_t dictlen = sizeof(DictionaryHeader);
    add_to_dict(&primary->source_id,    bundle->source_,    &tuples, &dictlen);
    add_to_dict(&primary->dest_id,      bundle->dest_,      &tuples, &dictlen);
    add_to_dict(&primary->custodian_id, bundle->custodian_, &tuples, &dictlen);
    add_to_dict(&primary->replyto_id,   bundle->replyto_,   &tuples, &dictlen);
                      
    //
    // dictionary header (must follow the primary)
    //
    DictionaryHeader* dictionary = (DictionaryHeader*)malloc(dictlen);
    int ntuples = tuples.size();
    dictionary->string_count = ntuples;
    
    u_char* records = &dictionary->records[0];
    for (int i = 0; i < ntuples; ++i) {
        *records++ = tuples[i].length();
        memcpy(records, tuples[i].data(), tuples[i].length());
        records += tuples[i].length();
    }

    *next_header_type = HEADER_DICTIONARY;
    next_header_type = &dictionary->next_header_type;
    iov[*iovcnt].iov_base = (char*)dictionary;
    iov[*iovcnt].iov_len  = dictlen;
    (*iovcnt)++;

    //
    // fragment header (if it's a fragment)
    //
    if (bundle->is_fragment_) {
        FragmentHeader* fragment = (FragmentHeader*)malloc(sizeof(FragmentHeader));
        u_int32_t orig_length = htonl(bundle->orig_length_);
        u_int32_t frag_offset = htonl(bundle->frag_offset_);
        memcpy(&fragment->orig_length, &orig_length, sizeof(orig_length));
        memcpy(&fragment->frag_offset, &frag_offset, sizeof(frag_offset));

        *next_header_type = HEADER_FRAGMENT;
        next_header_type = &fragment->next_header_type;
        iov[*iovcnt].iov_base = (char*)fragment;
        iov[*iovcnt].iov_len  = sizeof(FragmentHeader);
        (*iovcnt)++;
    }

    //
    // payload header (must be last)
    //
    PayloadHeader* payload = (PayloadHeader*)malloc(sizeof(PayloadHeader));
    u_int32_t payloadlen = htonl(bundle->payload_.length());
    memcpy(&payload->length, &payloadlen, 4);
    
    *next_header_type = HEADER_PAYLOAD;
    payload->next_header_type = HEADER_NONE;
    iov[*iovcnt].iov_base = (char*)payload;
    iov[*iovcnt].iov_len  = sizeof(PayloadHeader);
    (*iovcnt)++;

    // calculate the total length and then we're done
    int total_len = 0;
    for (int i = 0; i < *iovcnt; i++) {
        total_len += iov[i].iov_len;
    }
    return total_len;
}

void
BundleProtocol::free_header_iovmem(const Bundle* bundle, struct iovec* iov, int iovcnt)
{
    for (int i = 0; i < iovcnt; ++i) {
        free(iov[i].iov_base);
    }
}

int
BundleProtocol::parse_headers(Bundle* bundle, u_char* buf, size_t len)
{
    static const char* log = "/bundle/protocol";
    size_t origlen = len;
    u_int8_t next_header_type;

    //
    // primary header
    //
    PrimaryHeader* primary;
    if (len < sizeof(PrimaryHeader)) {
 tooshort:
        log_err(log, "bundle too short (length %d)", len);
        return -1;
    }
    
    primary = (PrimaryHeader*)buf;
    buf += sizeof(PrimaryHeader);
    len -= sizeof(PrimaryHeader);

    if (primary->version != CURRENT_VERSION) {
        log_warn(log, "protocol version mismatch %d != %d",
                 primary->version, CURRENT_VERSION);
        return -1;
    }

    // now pull out the fields from the primary header
    parse_cos(bundle, primary->class_of_service);
    parse_payload_security(bundle, primary->payload_security);
    bundle->expiration_  = ntohl(primary->expiration);
    get_timestamp(&bundle->creation_ts_, &primary->creation_ts);

    //
    // dictionary header (must follow primary)
    //
    DictionaryHeader* dictionary;
    if (primary->next_header_type != HEADER_DICTIONARY) {
        log_err(log, "dictionary header doesn't follow primary");
        return -1;
    }

    if (len < sizeof(DictionaryHeader)) {
        goto tooshort;
    }

    // grab the header and advance the pointers
    dictionary = (DictionaryHeader*)buf;
    next_header_type = dictionary->next_header_type;
    buf += sizeof(DictionaryHeader);
    len -= sizeof(DictionaryHeader);

    // pull out the tuple data into a local temporary array (there can
    // be at most 8 entries)
    u_char* tupledata[8];
    size_t  tuplelen[8];
    
    for (int i = 0; i < dictionary->string_count; ++i) {
        if (len < 1)
            goto tooshort;
        
        tuplelen[i] = *buf++;
        len--;
        
        if (len < tuplelen[i])
            goto tooshort;
        
        tupledata[i] = buf;
        buf += tuplelen[i];
        len -= tuplelen[i];
    }

    // now, pull out the tuples
    u_int8_t region_id, admin_id;

    AddressFamily* fixed_family =
        AddressFamilyTable::instance()->fixed_family();
    
#define EXTRACT_DICTIONARY_TUPLE(_what)                                 \
    region_id = primary->_what##id >> 4;                                \
    admin_id  = primary->_what##id & 0xf;                               \
    ASSERT((region_id <= 0xf) && (admin_id <= 0xf));                    \
                                                                        \
    if (bundle->_what.family() == fixed_family)                         \
    {                                                                   \
        PANIC("fixed family transmission not implemented");             \
    }                                                                   \
                                                                        \
    bundle->_what.assign(tupledata[region_id], tuplelen[region_id],     \
                         tupledata[admin_id],  tuplelen[admin_id]);     \
                                                                        \
    if (! bundle->_what.valid()) {                                      \
        log_err(log, "invalid %s tuple '%s'", #_what,                   \
                bundle->_what.c_str());                                 \
        return -1;                                                      \
    }                                                                   \
    log_debug(log, "parsed %s tuple (ids %d, %d) %s", #_what,           \
              region_id, admin_id, bundle->_what.c_str());

    EXTRACT_DICTIONARY_TUPLE(source_);
    EXTRACT_DICTIONARY_TUPLE(dest_);
    EXTRACT_DICTIONARY_TUPLE(custodian_);
    EXTRACT_DICTIONARY_TUPLE(replyto_);

    // start a loop until we've consumed all the other headers
    while (next_header_type != HEADER_NONE) {
        switch (next_header_type) {
        case HEADER_PRIMARY:
            log_err(log, "found a second primary header");
            return -1;

        case HEADER_DICTIONARY:
            log_err(log, "found a second dictionary header");
            return -1;

        case HEADER_FRAGMENT: {
            u_int32_t orig_length, frag_offset;
            
            if (len < sizeof(FragmentHeader)) {
                goto tooshort;
            }
            
            FragmentHeader* fragment = (FragmentHeader*)buf;
            buf += sizeof(FragmentHeader);
            len -= sizeof(FragmentHeader);
            next_header_type = fragment->next_header_type;
            
            bundle->is_fragment_ = true;
            memcpy(&orig_length, &fragment->orig_length, sizeof(orig_length));
            memcpy(&frag_offset, &fragment->frag_offset, sizeof(frag_offset));
            bundle->orig_length_ = htonl(orig_length);
            bundle->frag_offset_ = htonl(frag_offset);
            break;
        }

        case HEADER_PAYLOAD: {
            u_int32_t payload_len;
            
            if (len < sizeof(PayloadHeader)) {
                goto tooshort;
            }

            PayloadHeader* payload = (PayloadHeader*)buf;
            buf += sizeof(PayloadHeader);
            len -= sizeof(PayloadHeader);

            if (payload->next_header_type != HEADER_NONE) {
                log_err(log, "payload header must be last (next type %d)",
                        payload->next_header_type);
                return -1;
            }

            next_header_type = payload->next_header_type;
            
            memcpy(&payload_len, &payload->length, 4);
            bundle->payload_.set_length(ntohl(payload_len));

            log_debug(log, "parsed payload length %d",
                      bundle->payload_.length());
            break;
        }

        default:
            log_err(log, "unknown header type %d", next_header_type);
            return -1;
        }
    }
    
    // that's all we parse, return the amount we consumed
    return origlen - len;
}

/*
 * Conversion arrays.
 */
extern "C" u_long ustotslo[];
extern "C" u_long ustotsmid[];
extern "C" u_long ustotshi[];
extern "C" long tstouslo[];
extern "C" long tstousmid[];
extern "C" long tstoushi[];

/**
 * Store a struct timeval into a 64-bit NTP timestamp, suitable
 * for transmission over the network. This does not require that
 * the u_int64_t* be word-aligned.
 *
 * Implementation adapted from the NTP source distribution.
 */
    
void
BundleProtocol::set_timestamp(u_int64_t* ts, const struct timeval* tv)
{
    u_int32_t tsf = ustotslo[tv->tv_usec & 0xff] \
                    + ustotsmid[(tv->tv_usec >> 8) & 0xff] \
                    + ustotshi[(tv->tv_usec >> 16) & 0xf];
    
    u_int64_t ts_tmp = (((u_int64_t)ntohl(tv->tv_sec)) << 32) | ntohl(tsf);
    memcpy(ts, &ts_tmp, sizeof(*ts));
}

/**
 * Retrieve a struct timeval from a 64-bit NTP timestamp that was
 * transmitted over the network. This does not require that the
 * u_int64_t* be word-aligned.
 *
 * Implementation adapted from the NTP source distribution.
 */
void
BundleProtocol::get_timestamp(struct timeval* tv, const u_int64_t* ts)
{
#define TV_SHIFT        3
#define TV_ROUNDBIT     0x4
    
    u_int64_t ts_tmp;
    memcpy(&ts_tmp, ts, sizeof(ts_tmp));

    tv->tv_sec = ntohl(ts_tmp >> 32);

    u_int32_t tsf = ntohl((u_int32_t)ts_tmp);
    tv->tv_usec = (tstoushi[((tsf) >> 24) & 0xff] \
                   + tstousmid[((tsf) >> 16) & 0xff] \
                   + tstouslo[((tsf) >> 9) & 0x7f] \
                   + TV_ROUNDBIT) >> TV_SHIFT;

    if ((tv)->tv_usec == 1000000) {
        (tv)->tv_sec++;
        (tv)->tv_usec = 0;
    }
}

u_int8_t
BundleProtocol::format_cos(const Bundle* b)
{
    u_int8_t cos = 0;

    if (b->custreq_)
        cos |= (1 << 7);

    cos |= ((b->priority_ & 0x7) << 4);
    
    if (b->return_rcpt_)
        cos |= (1 << 3);

    if (b->fwd_rcpt_)
        cos |= (1 << 2);

    if (b->recv_rcpt_)
        cos |= (1 << 1);

    if (b->custody_rcpt_)
        cos |= 1;

    return cos;
}

void
BundleProtocol::parse_cos(Bundle* b, u_int8_t cos)
{
    if (cos & (1 << 7))
        b->custreq_ = true;

    b->priority_ = ((cos >> 4) & 0x7);

    if (cos & (1 << 3))
        b->return_rcpt_ = true;

    if (cos & (1 << 2))
        b->fwd_rcpt_ = true;

    if (cos & (1 << 1))
        b->recv_rcpt_ = true;
    
    if (cos & 1)
        b->custody_rcpt_ = true;
}

u_int8_t
BundleProtocol::format_payload_security(const Bundle* bundle)
{
    // TBD
    return 0;
}

void
BundleProtocol::parse_payload_security(Bundle* bundle,
                                       u_int8_t payload_security)
{
}


} // namespace dtn

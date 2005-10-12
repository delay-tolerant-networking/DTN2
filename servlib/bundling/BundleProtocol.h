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
#ifndef _BUNDLE_PROTOCOL_H_
#define _BUNDLE_PROTOCOL_H_

#include <sys/types.h>

namespace dtn {

class Bundle;

/**
 * Class used to convert a Bundle to / from the bundle protocol
 * specification for the "on-the-wire" representation.
 */
class BundleProtocol {
public:
    /**
     * Fill in the given buffer with the formatted bundle headers
     * including the payload header, but not the payload data.
     *
     * @return the total length of the header or -1 on error (i.e. too
     * small of a buffer)
     */
    static int format_headers(const Bundle* bundle, u_char* buf, size_t len);
    
    /**
     * Parse the header data, filling in the bundle. Note that this
     * implementation doesn't support sizes bigger than 4GB for the
     * headers.
     *
     * @return the length consumed or -1 on error (i.e. not enough
     * data in buffer)
     */
    static int parse_headers(Bundle* bundle, u_char* buf, size_t len);

    /**
     * Store a struct timeval into a 64-bit timestamp suitable for
     * transmission over the network.
     */
    static void set_timestamp(u_char* ts, const struct timeval* tv);
    
    /**
     * Store a struct timeval into a 64-bit timestamp suitable for
     * transmission over the network. This doesn't require the 64-bit
     * destination to be word-aligned
     */
    static void set_timestamp(u_int64_t* ts, const struct timeval* tv) {
        set_timestamp((u_char *) ts, tv);
    }
    
    /**
     * Retrieve a struct timeval from a 64-bit timestamp that was
     * transmitted over the network. This does not require the
     * timestamp to be word-aligned.
     */
    static void get_timestamp(struct timeval* tv, const u_char* ts);

    /**
     * Retrieve a struct timeval from a 64-bit timestamp that was
     * transmitted over the network. This does not require the
     * timestamp to be word-aligned.
     */
    static void get_timestamp(struct timeval* tv, const u_int64_t* ts) {
        get_timestamp(tv,(u_char *) ts);
    }

    /**
     * The current version of the bundling protocol.
     */
    static const int CURRENT_VERSION = 0x04;

    /**
     * Values for bundle processing flags that appear in the primary
     * header.
     */
    typedef enum {
        BUNDLE_IS_FRAGMENT      = 0x1,
        BUNDLE_IS_ADMIN         = 0x2,
        BUNDLE_DO_NOT_FRAGMENT  = 0x4
    } bundle_flag_t;

    /**
     * The first fixed-field portion of the primary bundle header
     * preamble structure.
     */
    struct PrimaryHeader1 {
        u_int8_t  version;
        u_int8_t  processing_flags;
        u_int8_t  class_of_service;
        u_char    header_length[0]; // SDNV
        
    } __attribute__((packed));

    /**
     * The remainder of the fixed-length part of the primary bundle
     * header that immediately follows the header length.
     */
    struct PrimaryHeader2 {
        u_int16_t dest_scheme_offset;
        u_int16_t dest_ssp_offset;
        u_int16_t source_scheme_offset;
        u_int16_t source_ssp_offset;
        u_int16_t replyto_scheme_offset;
        u_int16_t replyto_ssp_offset;
        u_int16_t custodian_scheme_offset;
        u_int16_t custodian_ssp_offset;
        u_int64_t creation_ts;
        u_int32_t lifetime;
        
    } __attribute__((packed));

    /**
     * Valid type codes for bundle headers (other than payload).
     */
    typedef enum {
        HEADER_PAYLOAD          = 0x01
    } bundle_header_type_t;
    
    /**
     * Values for header processing flags that appear in all headers
     * except the primary header.
     */
    typedef enum {
        HEADER_FLAG_REPLICATE           = 0x1,
        HEADER_FLAG_REPORT_ONERROR      = 0x2,
        HEADER_FLAG_DISCARD_ONERROR     = 0x4
    } header_flag_t;
    
    /**
     * The basic header preamble that's common to all headers
     * (including the payload header but not the primary header).
     */
    struct HeaderPreamble {
        u_int8_t type;
        u_int8_t flags;
        u_char length[0]; // SDNV
        
    } __attribute__((packed));
     
    /**
     * Valid type codes for administrative payloads.
     */
    typedef enum {
        ADMIN_STATUS_REPORT     = 0x01,
        ADMIN_CUSTODY_SIGNAL    = 0x02,
        ADMIN_ECHO              = 0x03,
        ADMIN_NULL              = 0x04,
    } admin_payload_type_t;

    /**
     * Valid status flags.
     */
    typedef enum {
        STATUS_RECEIVED         = 0x01,
        STATUS_CUSTODY_ACCEPTED = 0x02,
        STATUS_FORWARDED        = 0x04,
        STATUS_DELIVERED        = 0x08,
        STATUS_DELETED  	= 0x10,
        STATUS_ACKED_BY_APP	= 0x20,
        STATUS_UNUSED		= 0x40,
        STATUS_FRAGMENT		= 0x80,
    } status_report_flag_t;

    /**
     * Bundle Status Report "Reason Code" flags
     */
    typedef enum {
	REASON_NO_ADDTL_INFO              = 0X00,
	REASON_LIFETIME_EXPIRED           = 0X01,
	REASON_FORWARDED_UNIDIR_LINK      = 0X02,
	REASON_TRANSMISSION_CANCELLED     = 0X03,
	REASON_DEPLETED_STORAGE           = 0X04,
	REASON_ENDPOINT_ID_UNINTELLIGIBLE = 0X05,
	REASON_NO_ROUTE_TO_DEST           = 0X06,
	REASON_NO_TIMELY_CONTACT          = 0X07,
	REASON_HEADER_UNINTELLIGIBLE      = 0X08,
    } status_report_reason_t;
	
protected:
    static u_int8_t format_processing_flags(const Bundle* bundle);
    static void parse_processing_flags(Bundle* bundle, u_int8_t flags);
    
    static u_int8_t format_cos(const Bundle* bundle);
    static void parse_cos(Bundle* bundle, u_int8_t cos);
};

} // namespace dtn

#endif /* _BUNDLE_PROTOCOL_H_ */

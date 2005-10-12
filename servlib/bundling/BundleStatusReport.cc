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

#include "BundleStatusReport.h"
#include "SDNV.h"
#include <netinet/in.h>
#include <oasys/util/ScratchBuffer.h>

namespace dtn {

BundleStatusReport::BundleStatusReport(Bundle* orig_bundle,
                                       const EndpointID& source,
                                       BundleProtocol::status_report_flag_t flag,
                                       BundleProtocol::status_report_reason_t reason)
    : Bundle()
{
    source_.assign(source);
    replyto_.assign(source);
    custodian_.assign(source);
    dest_.assign(orig_bundle->replyto_);
    is_admin_ = true;

    // use the expiration time from the original bundle
    // XXX/demmer maybe something more clever??
    expiration_ = orig_bundle->expiration_;

    // store the original bundle's source eid
    EndpointID orig_source(orig_bundle->source_);

    u_char status_flags = flag; // for building up the final status_flags byte
    int sdnv_encoding_len = 0;  // use an int to handle -1 return values
    int report_length = 0;
    
    // we generally don't expect the Status Peport length to be > 256 bytes
    oasys::ScratchBuffer<u_char*, 256> scratch;

    //
    // Calculate Report Size and Set Fragment Status Flag:
    //
    
    // Structure of this implementation's Bundle X Status Report Payload:
    
    // 1 byte Admin Payload Type
    // 1 byte Status Flags
    // 1 byte Reason Code
    // SDNV   [Fragment Offset (if present)]
    // SDNV   [Fragment Length (if present)]
    // 8 byte Time of {receipt/forwarding/delivery/deletion/custody/app-ack}
    //        of bundle X
    // 8 byte Copy of bundle X's Creation Timestamp
    // SDNV   Length of X's source endpoint ID
    // vari   Source endpoint ID of bundle X

    // note that the spec allows for all 6 of the "Time of..." fields
    // to be present in a single Status Report, but for this
    // implementation we will always have one and only one of the 6
    // timestamp fields
    // XXX/matt we may want to change to allow multiple-timestamps per SR

    // the non-optional, fixed-length fields above:
    report_length = 1 + 1 + 1 + 8 + 8;

    // the 2 SDNV fragment fields:
    if (orig_bundle->is_fragment_) {
        report_length += SDNV::encoding_len(orig_bundle->frag_offset_);
        report_length += SDNV::encoding_len(orig_bundle->payload_.length());
        status_flags |= BundleProtocol::STATUS_FRAGMENT;
    }

    // the Source Endpoint ID:
    report_length += SDNV::encoding_len(orig_source.length()) + orig_source.length();

    //
    // Done calculating length, now create the report payload
    //
    
    u_char* bp = scratch.buf(report_length);
    int len = report_length;
    
    // Admin Payload Type
    *bp++ = BundleProtocol::ADMIN_STATUS_REPORT;
    len--;

    // Status Flags
    *bp++ = status_flags;
    len--;

    // Reason Code
    *bp++ = reason;
    len--;
    
    // The 2 Fragment Fields
    if (orig_bundle->is_fragment_) {
        sdnv_encoding_len = SDNV::encode(orig_bundle->frag_offset_, bp, len);
        ASSERT(sdnv_encoding_len > 0);
        bp  += sdnv_encoding_len;
        len -= sdnv_encoding_len;
        
        sdnv_encoding_len = SDNV::encode(orig_bundle->payload_.length(), bp, len);
        ASSERT(sdnv_encoding_len > 0);
        bp  += sdnv_encoding_len;
        len -= sdnv_encoding_len;
    }   

    // Time field, set to the current time:
    struct timeval now;
    gettimeofday(&now, 0);
    BundleProtocol::set_timestamp(bp, &now);
    len -= sizeof(u_int64_t);
    bp  += sizeof(u_int64_t);

    // Copy of bundle X's Creation Timestamp
    BundleProtocol::set_timestamp(bp, &orig_bundle->creation_ts_);
    len -= sizeof(u_int64_t);
    bp  += sizeof(u_int64_t);
    
    // The 2 Endpoint ID fields:
    sdnv_encoding_len = SDNV::encode(orig_source.length(), bp, len);
    ASSERT(sdnv_encoding_len > 0);
    len -= sdnv_encoding_len;
    bp  += sdnv_encoding_len;
    
    ASSERT((u_int)len == orig_source.length());
    memcpy(bp, orig_source.c_str(), orig_source.length());
    
    // 
    // Finished generating the payload
    //
    payload_.set_data(scratch.buf(), report_length);
}

/**
 * Parse a byte stream containing a Status Report Payload and store
 * the fields in the given struct. Returns false if parsing failed.
 */
bool BundleStatusReport::parse_status_report(status_report_data_t* data,
                                             const u_char* bp, u_int len)
{
    // 1 byte Admin Payload Type:
    if (len < 1) { return false; }
    data->admin_type_ = *bp++;
    len--;
        
    // 1 byte Status Flags:
    if (len < 1) { return false; }
    data->status_flags_ = *bp++;
    len--;
    
    // 1 byte Reason Code:
    if (len < 1) { return false; }
    data->reason_code_ = *bp++;
    len--;

    // Fragment SDNV Fields (offset & length), if present:
    if (data->status_flags_ & BundleProtocol::STATUS_FRAGMENT) {
        int sdnv_bytes = SDNV::decode(bp, len, &data->frag_offset_);
        if (sdnv_bytes == -1) { return false; }
        bp  += sdnv_bytes;
        len -= sdnv_bytes;
        sdnv_bytes = SDNV::decode(bp, len, &data->frag_length_);
        if (sdnv_bytes == -1) { return false; }
        bp  += sdnv_bytes;
        len -= sdnv_bytes;
    }

    // The 6 Optional ACK Timestamps:
    
    if (data->status_flags_ & BundleProtocol::STATUS_RECEIVED) {
        if (len < sizeof(u_int64_t)) { return false; }
        BundleProtocol::get_timestamp(&data->receipt_tv_, bp);
        bp  += sizeof(u_int64_t);
        len -= sizeof(u_int64_t);
    }

    if (data->status_flags_ & BundleProtocol::STATUS_CUSTODY_ACCEPTED) {
        if (len < sizeof(u_int64_t)) { return false; }
        BundleProtocol::get_timestamp(&data->custody_tv_, bp);
        bp  += sizeof(u_int64_t);
        len -= sizeof(u_int64_t);
    }

    if (data->status_flags_ & BundleProtocol::STATUS_FORWARDED) {
        if (len < sizeof(u_int64_t)) { return false; }
        BundleProtocol::get_timestamp(&data->forwarding_tv_, bp);
        bp  += sizeof(u_int64_t);
        len -= sizeof(u_int64_t);
    }

    if (data->status_flags_ & BundleProtocol::STATUS_DELIVERED) {
        if (len < sizeof(u_int64_t)) { return false; }
        BundleProtocol::get_timestamp(&data->delivery_tv_, bp);
        bp  += sizeof(u_int64_t);
        len -= sizeof(u_int64_t);
    }

    if (data->status_flags_ & BundleProtocol::STATUS_DELETED) {
        if (len < sizeof(u_int64_t)) { return false; }
        BundleProtocol::get_timestamp(&data->deletion_tv_, bp);
        bp  += sizeof(u_int64_t);
        len -= sizeof(u_int64_t);
    }

    if (data->status_flags_ & BundleProtocol::STATUS_ACKED_BY_APP) {
        if (len < sizeof(u_int64_t)) { return false; }
        BundleProtocol::get_timestamp(&data->acknowledgement_tv_, bp);
        bp  += sizeof(u_int64_t);
        len -= sizeof(u_int64_t);
    }

    
    // Bundle Creation Timestamp
    if (len < sizeof(u_int64_t)) { return false; }
    BundleProtocol::get_timestamp(&data->creation_tv_, bp);
    bp  += sizeof(u_int64_t);
    len -= sizeof(u_int64_t);

    
    // EID of Bundle
    u_int64_t EID_len;
    int num_bytes = SDNV::decode(bp, len, &EID_len);
    if (num_bytes == -1) { return false; }
    bp  += num_bytes;
    len -= num_bytes;

    if (len != EID_len) { return false; }
    // XXX/matt OK to convert u_char to char here?
    data->EID_.append((const char*)bp, len);

    return true;
    
}

} // namespace dtn

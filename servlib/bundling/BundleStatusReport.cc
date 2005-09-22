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
#include <oasys/util/StaticScratchBuffer.h>

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
    oasys::StaticScratchBuffer<256, u_char*> scratch;

    //
    // Calculate Report Size and Set Fragment Status Flag:
    //
    
    // Structure of this implementation's Bundle X Status Report Payload:
    
    // 1 byte Admin Payload Type
    // 1 byte Status Flags
    // 1 byte Reason Code
    // SDNV   [Fragment Offset (if present)]
    // SDNV   [Fragment Length (if present)]
    // 8 byte Time of {receipt/forwarding/delivery/deletion} of bundle X
    // 8 byte Copy of bundle X's Creation Timestamp
    // SDNV   Length of X's source endpoint ID
    // vari   Source endpoint ID of bundle X

    // note that the spec allows for all 4 of the "Time of..." fields
    // to be present, but for this implementation we will always have
    // one and only one of the 4 timestamp fields in our Status Report

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
    bp++;
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

} // namespace dtn

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

namespace dtn {

BundleStatusReport::BundleStatusReport(Bundle* orig_bundle,
                                       const EndpointID& source)
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
    orig_source_.assign(orig_bundle->source_);
    
    // the report's length is variable since we need to encode the
    // original bundle's source endpoint id, so precalculate its
    // length here
    int sdnv_len = SDNV::encoding_len(orig_source_.length());
    report_len_ = sizeof(BundleProtocol::StatusReport) +
                  sdnv_len + orig_source_.length();

    report_ = (BundleProtocol::StatusReport*)malloc(report_len_);
    memset(report_, 0, report_len_);
    
    report_->admin_type = BundleProtocol::ADMIN_STATUS_REPORT;
    report_->orig_ts = ((u_int64_t)htonl(orig_bundle->creation_ts_.tv_sec)) << 32;
    report_->orig_ts |= (u_int64_t)htonl(orig_bundle->creation_ts_.tv_usec);
}

BundleStatusReport::~BundleStatusReport()
{
    free(report_);
}

/**
 * Sets the appropriate status report flag and timestamp to the
 * current time.
 */
void
BundleStatusReport::set_status(BundleProtocol::status_report_flag_t flag)
{
    u_int64_t* ts = 0;
    
    switch(flag) {
    case BundleProtocol::STATUS_RECEIVED:
        ts = &report_->receipt_ts;
        break;
            
    case BundleProtocol::STATUS_CUSTODY_ACCEPTED:
        ts = &report_->receipt_ts;
        break;
                
    case BundleProtocol::STATUS_FORWARDED:
        ts = &report_->forward_ts;
        break;
                    
    case BundleProtocol::STATUS_DELIVERED:
        ts = &report_->delivery_ts;
        break;
        
    case BundleProtocol::STATUS_TTL_EXPIRED:
        ts = &report_->delete_ts;
        break;
                            
    case BundleProtocol::STATUS_UNAUTHENTIC:
    case BundleProtocol::STATUS_UNUSED:
    case BundleProtocol::STATUS_FRAGMENT:
        NOTIMPLEMENTED;
    }

    struct timeval now;
    gettimeofday(&now, 0);

    report_->status_flags |= flag;
    BundleProtocol::set_timestamp(ts, &now);
}

/**
 * Stores the completed report into the bundle payload. Must be
 * called before transmitting the bundle.
 */
void
BundleStatusReport::generate_payload()
{
    /*
     * Copy out the variable length part (the source endpoint id's
     * length (as an sdnv) and value.
     */
    size_t len = report_len_ - sizeof(BundleProtocol::StatusReport);
    u_char* bp = &report_->source_eid_data[0];
    
    int sdnv_len = SDNV::encode(orig_source_.length(), bp, len);
    ASSERT(sdnv_len > 0);
    
    len -= sdnv_len;
    bp += sdnv_len;
    ASSERT(len == orig_source_.length());
    memcpy(bp, orig_source_.c_str(), orig_source_.length());
        
    payload_.set_data((const u_char*)report_, report_len_);
}

} // namespace dtn

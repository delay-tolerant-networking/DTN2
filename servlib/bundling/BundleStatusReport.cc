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
#include "BundleProtocol.h"
#include <netinet/in.h>

BundleStatusReport::BundleStatusReport(Bundle* orig_bundle, BundleTuple& source)
    : Bundle()
{
    source_.assign(source);
    replyto_.assign(source);
    custodian_.assign(source);

    dest_.assign(orig_bundle->replyto_);

    // the payload's length is variable based on the original bundle
    // source's region and admin length
    size_t region_len = orig_bundle->source_.region().length();
    size_t admin_len =  orig_bundle->source_.admin().length();
    
    size_t len = sizeof(BundleStatusReport) + 2 + region_len + admin_len;

    payload_.set_length(len, BundlePayload::MEMORY);
    ASSERT(payload_.location() == BundlePayload::MEMORY);
    
    StatusReport* report = (StatusReport*)payload_.memory_data();
    memset(report, 0, sizeof(StatusReport));

    report->admin_type = ADMIN_STATUS_REPORT;
    report->orig_ts = ((u_int64_t)htonl(orig_bundle->creation_ts_.tv_sec)) << 32;
    report->orig_ts |= (u_int64_t)htonl(orig_bundle->creation_ts_.tv_usec);
    
    char* bp = &report->tuple_data[0];
    *bp = region_len;
    memcpy(&bp[1], orig_bundle->source_.region().data(), region_len);
    bp += 1 + region_len;
 
    *bp = admin_len;
    memcpy(&bp[1], orig_bundle->source_.admin().data(), admin_len);
    bp += 1 + admin_len;
}

/**
 * Sets the appropriate status report flag and timestamp to the
 * current time.
 */
void
BundleStatusReport::set_status_time(status_report_flag_t flag)
{
    StatusReport* report = (StatusReport*)payload_.memory_data();

    u_int64_t* ts;
    
    switch(flag) {
    case STATUS_RECEIVED:
        ts = &report->receipt_ts;
        break;
            
    case STATUS_CUSTODY_ACCEPTED:
        ts = &report->receipt_ts;
        break;
                
    case STATUS_FORWARDED:
        ts = &report->forward_ts;
        break;
                    
    case STATUS_DELIVERED:
        ts = &report->delivery_ts;
        break;
        
    case STATUS_TTL_EXPIRED:
        ts = &report->delete_ts;
        break;
                            
    case STATUS_UNAUTHENTIC:
    case STATUS_UNUSED:
    case STATUS_FRAGMENT:
        NOTIMPLEMENTED;
    }

    struct timeval now;
    gettimeofday(&now, 0);

    report->status_flags |= flag;
    *ts = ((u_int64_t)htonl(now.tv_sec)) << 32;
    *ts	|= (u_int64_t)htonl(now.tv_usec);
}

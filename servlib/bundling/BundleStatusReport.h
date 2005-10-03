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
#ifndef _BUNDLE_STATUS_REPORT_H_
#define _BUNDLE_STATUS_REPORT_H_

#include "Bundle.h"
#include "BundleProtocol.h"

namespace dtn {

/**
 * Specification of the contents of a DTN Bundle Status Report
 */
struct status_report_data_t {
    u_char                admin_type_;
    u_char                status_flags_;
    u_char                reason_code_;
    u_int64_t             frag_offset_;
    u_int64_t             frag_length_;
    timeval               receipt_tv_;
    timeval               forwarding_tv_;
    timeval               delivery_tv_;
    timeval               deletion_tv_;
    timeval               creation_tv_;
    oasys::StringBuffer   EID_;
};
typedef struct status_report_data_t status_report_data_t;

/**
 * A special bundle class whose payload is a status report. Used at
 * the source to generate the report.
 */
class BundleStatusReport : public Bundle {
public:
    /**
     * Constructor.
     */
    BundleStatusReport(Bundle* orig_bundle,
                       const EndpointID& source,
                       BundleProtocol::status_report_flag_t flag,
                       BundleProtocol::status_report_reason_t reason);

    /**
     * Parse a byte stream containing a Status Report Payload and
     * store the fields in the given struct. Returns false if parsing
     * failed
     */
    static bool parse_status_report(status_report_data_t* data,
                                    const u_char* bp, u_int len);
};

} // namespace dtn

#endif /* _BUNDLE_STATUS_REPORT_H_ */

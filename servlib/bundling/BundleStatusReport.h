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
 * Utility class to create and parse status reports.
 */
class BundleStatusReport {
public:
    /**
     * The status report flags are defined in the BundleProtocol class.
     */
    typedef BundleProtocol::status_report_flag_t flag_t;
    
    /**
     * The reason codes are defined in the BundleProtocol class.
     */
    typedef BundleProtocol::status_report_reason_t reason_t;

    /**
     * Specification of the contents of a Bundle Status Report
     */
    struct data_t {
        u_int8_t       admin_type_;
        u_int8_t       admin_flags_;
        u_int8_t       status_flags_;
        u_int8_t       reason_code_;
        u_int64_t      orig_frag_offset_;
        u_int64_t      orig_frag_length_;
        timeval        receipt_tv_;
        timeval        custody_tv_;
        timeval        forwarding_tv_;
        timeval        delivery_tv_;
        timeval        deletion_tv_;
        timeval        acknowledgement_tv_;
        timeval        orig_creation_tv_;
        EndpointID     orig_source_eid_;
    };

    /**
     * Constructor-like function that fills in the bundle payload
     * buffer with the appropriate status report format.
     *
     * Although the spec allows for multiple timestamps to be set in a
     * single status report, this implementation only supports
     * creating a single timestamp per report, hence there is only
     * support for a single flag to be passed in the parameters.
     */
    static void create_status_report(Bundle*           bundle,
                                     const Bundle*     orig_bundle,
                                     const EndpointID& source,
                                     flag_t            status_flag,
                                     reason_t          reason);
    
    /**
     * Parse a byte stream containing a Status Report Payload and
     * store the fields in the given struct. Returns false if parsing
     * failed.
     */
    static bool parse_status_report(data_t* data,
                                    const u_char* bp, u_int len);
};

} // namespace dtn

#endif /* _BUNDLE_STATUS_REPORT_H_ */

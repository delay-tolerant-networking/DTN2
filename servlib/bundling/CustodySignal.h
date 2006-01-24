/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 *
 * Intel Open Source License
 *
 * 2006 Intel Corporation. All rights reserved.
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
#ifndef _CUSTODYSIGNAL_H_
#define _CUSTODYSIGNAL_H_

#include "Bundle.h"
#include "BundleProtocol.h"

namespace dtn {

/**
 * Utility class to format and parse custody signal bundles.
 */
class CustodySignal {
public:
    /**
     * The reason codes are defined in the bundle protocol class.
     */
    typedef BundleProtocol::custody_signal_reason_t reason_t;

    /**
     * Struct to hold the payload data of the custody signal.
     */
    struct data_t {
        u_int8_t   admin_type_;
        u_int8_t   admin_flags_;
        bool       succeeded_;
        u_int8_t   reason_;
        u_int64_t  orig_frag_offset_;
        u_int64_t  orig_frag_length_;
        timeval    custody_signal_tv_;
        timeval    orig_creation_tv_;
        EndpointID orig_source_eid_;
    };

    /**
     * Constructor-like function to create a new custody signal bundle.
     */
    static void create_custody_signal(Bundle*           bundle,
                                      const Bundle*     orig_bundle,
                                      const EndpointID& source_eid,
                                      bool              succeeded,
                                      reason_t          reason);

    /**
     * Parsing function for custody signal bundles.
     */
    static bool parse_custody_signal(data_t* data,
                                     const u_char* bp, u_int len);
};

} // namespace dtn

#endif /* _CUSTODYSIGNAL_H_ */

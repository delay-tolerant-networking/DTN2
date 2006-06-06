/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2006 Intel Corporation. All rights reserved. 
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
#ifndef _BUNDLETIMESTAMP_H_
#define _BUNDLETIMESTAMP_H_

#include <sys/types.h>

namespace dtn {

/**
 * Simple struct definition for bundle creation timestamps. Although
 * quite similar to a struct timeval, the bundle protocol
 * specification mandates that timestamps should be calculated as
 * seconds since Jan 1, 2000 (not 1970) so we use a different type.
 */
struct BundleTimestamp {
    u_int32_t seconds_; ///< Seconds since 1/1/2000
    u_int32_t seqno_;   ///< Sub-second sequence number
    
    /**
     * Return the current time in the correct format for the bundle
     * protocol, i.e. seconds since Jan 1, 2000 in UTC.
     */
    static u_int32_t get_current_time();

    /**
     * Check that the local clock setting is valid (i.e. is at least
     * up to date with the protocol.
     */
    static bool check_local_clock();

    /**
     * The number of seconds between 1/1/1970 and 1/1/2000.
     */
    static const u_int32_t TIMEVAL_CONVERSION = 946684800;
};

} // namespace dtn

#endif /* _BUNDLETIMESTAMP_H_ */

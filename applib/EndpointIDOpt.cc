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

#include "EndpointIDOpt.h"
#include "dtn_api.h"

namespace dtn {

EndpointIDOpt::EndpointIDOpt(const char* opt, dtn_endpoint_id_t* valp,
                             const char* valdesc, const char* desc, bool* setp)
    : Opt(0, opt, valp, setp, true, valdesc, desc)
{
}

EndpointIDOpt::EndpointIDOpt(char shortopt, const char* longopt,
                             dtn_endpoint_id_t* valp,
                             const char* valdesc, const char* desc, bool* setp)
    : Opt(shortopt, longopt, valp, setp, true, valdesc, desc)
{
}

int
EndpointIDOpt::set(const char* val, size_t len)
{
    char buf[DTN_MAX_ENDPOINT_ID];
    if (len > (DTN_MAX_ENDPOINT_ID - 1)) {
        return -1;
    }
    
    memcpy(buf, val, len);
    buf[len] = '\0';

    int err = dtn_parse_eid_string(((dtn_endpoint_id_t*)valp_), buf);
    if (err != 0) {
        return -1;
    }

    return 0;
}

} // namespace dtn

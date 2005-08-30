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

#include <oasys/io/NetUtils.h>
#include "IPConvergenceLayer.h"

namespace dtn {

/**
 * Parse a next hop address specification of the form
 * <host>[:<port>?].
 *
 * @return true if the conversion was successful, false
 */
bool
IPConvergenceLayer::parse_nexthop(const char* nexthop,
                                  in_addr_t* addr, u_int16_t* port)
{
    const char* host;
    std::string tmp;

    *addr = INADDR_NONE;
    *port = 0;

    // first see if there's a colon in the string -- if so, it should
    // separate the hostname and port, if not, then the whole string
    // should be a hostname
    const char* colon = strchr(nexthop, ':');
    if (colon != NULL) {
        char* endstr;
        u_int32_t portval = strtoul(colon + 1, &endstr, 10);
        
        if (*endstr != '\0' || portval > 65535) {
            log_warn("invalid port %s in next hop '%s'",
                     colon + 1, nexthop);
            return false;
        }

        *port = (u_int16_t)portval;
        
        tmp.assign(nexthop, colon - nexthop);
        host = tmp.c_str();
    } else {
        host = nexthop;
    }

    // now look up the hostname
    if (oasys::gethostbyname(host, addr) != 0) {
        log_warn("invalid hostname '%s' in next hop %s",
                 host, nexthop);
        return false;
    }
    
    return true;
}
    

} // namespace dtn

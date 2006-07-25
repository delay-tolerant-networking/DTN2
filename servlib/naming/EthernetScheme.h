/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
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
#ifndef _ETHERNET_SCHEME_H_
#define _ETHERNET_SCHEME_H_

#ifdef __linux__

#include "Scheme.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <oasys/compat/inttypes.h>
#include <oasys/util/Singleton.h>

namespace dtn {

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN 6
#endif
    
typedef struct ether_addr {
    u_char octet[ETHER_ADDR_LEN];
} eth_addr_t;    

/**
 * Endpoint ID scheme to represent ethernet-style addresses.
 *
 * e.g:  eth://00:01:02:03:04:05
 */
class EthernetScheme : public Scheme, public oasys::Singleton<EthernetScheme> {
public:
    /**
     * Validate that the given ssp is legitimate for this scheme. If
     * the 'is_pattern' paraemeter is true, then the ssp is being
     * validated as an EndpointIDPattern.
     *
     * @return true if valid
     */
    virtual bool validate(const std::string& ssp, bool is_pattern = false);

    /**
     * Match the pattern to the endpoint id in a scheme-specific
     * manner.
     */
    virtual bool match(const EndpointIDPattern& pattern,
                       const EndpointID& eid);
    
    /*
     * Parse out an ethernet address from the ssp.
     *
     * @return true if the extraction was a success, false if
     * malformed
     */
    static bool parse(const std::string& ssp, eth_addr_t* addr);

    /**
     * Given an ethernet address, write out a string representation.
     * addr needs to point to a six-byte buffer that contains the address.
     * outstring needs to point to a buffer of length at least 23 chars. 
     * eth://00:00:00:00:00:00
     * Returns outstring. 
     */
    static char* to_string(u_int8_t* addr, char* outstring);

private:
    friend class oasys::Singleton<EthernetScheme>;
    EthernetScheme() {}
};
    
} // namespace dtn


#endif /* __linux__ */

#endif /* _ETHERNET_SCHEME_H_ */

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

#ifdef __linux__

#include <oasys/io/IPSocket.h>
#include <oasys/io/NetUtils.h>
#include <oasys/util/URL.h>

#include "EthernetScheme.h"
#include "EndpointID.h"

namespace dtn {

EthernetScheme* oasys::Singleton<EthernetScheme>::instance_;

/**
 * Given an admin string , parse out the ip address and port.
 * Potentially does a hostname lookup if the admin string doesn't
 * contain a specified IP address. If the url did not contain a
 * port, 0 is returned.
 *
 * @return true if the extraction was a success, false if the url
 * is malformed or is not in the internet address family.
 */
bool
EthernetScheme::parse(const std::string& ssp, eth_addr_t* addr)
{
    // XXX/jakob - for now, assume it's a correctly formatted ethernet URI
    // eth://00:01:02:03:04:05 or eth:00:01:02:03:04:05

    const char* str = ssp.c_str();
    if (str[0] == '/' && str[1] == '/') {
        str = str + 2;
    }
    
    // this is a nasty hack. grab the ethernet address out of there,
    // assuming everything is correct
    int r = sscanf(str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
                   &addr->octet[0],
                   &addr->octet[1],
                   &addr->octet[2],
                   &addr->octet[3],
                   &addr->octet[4],
                   &addr->octet[5]);
    if (r != 6) {
        log_err("/scheme/ethernet",
                "string %s is not a valid ethernet URL", ssp.c_str());
        return false;
    }

    return true;
}

/**
 * Validate that the given ssp is legitimate for this scheme. If
 * the 'is_pattern' paraemeter is true, then the ssp is being
 * validated as an EndpointIDPattern.
 *
 * @return true if valid
 */
bool
EthernetScheme::validate(const std::string& ssp, bool is_pattern)
{
    // make sure it's a valid url
    eth_addr_t addr;
    return parse(ssp, &addr);
}

/**
 * Match the given ssp with the given pattern.
 *
 * @return true if it matches
 */
bool
EthernetScheme::match(EndpointIDPattern* pattern,
                      const std::string& ssp)
{
    std::string pattern_ssp = pattern->ssp();
    
    log_debug("/scheme/ethernet",
              "matching %s against %s.", pattern_ssp.c_str(), ssp.c_str());

    size_t patternlen = pattern_ssp.length();
    
    if (pattern_ssp == ssp) 
        return true;
    
    if (patternlen >= 1 && pattern_ssp[patternlen-1] == '*') {
        patternlen--;
        
        if (pattern_ssp.substr(0, patternlen) ==
            ssp.substr(0, patternlen))
        {
            return true;
        }
    }

    return false;
}

char* 
EthernetScheme::to_string(eth_addr_t* addr, char* outstring)
{
    sprintf(outstring,"eth://%02X:%02X:%02X:%02X:%02X:%02X", 
            addr->octet[0], addr->octet[1],
            addr->octet[2], addr->octet[3],
            addr->octet[4], addr->octet[5]);
    
    return outstring;
}


} // namespace dtn

#endif /* __linux__ */

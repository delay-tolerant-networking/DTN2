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

#include <oasys/util/URL.h>
#include <oasys/io/NetUtils.h>

#include "InternetAddressFamily.h"

namespace dtn {

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
InternetAddressFamily::parse(const std::string& admin,
                             in_addr_t* addr, u_int16_t* port)
{
    // XXX/demmer validate that the AF in the tuple is in fact of type
    // InternetAddressFamily

    oasys::URL url(admin);
    if (! url.valid()) {
        log_debug("/af/internet",
                  "admin string '%s' not a valid url", admin.c_str());
        return false;
    }

    // look up the hostname in the url
    *addr = INADDR_NONE;
    if (oasys::gethostbyname(url.host_.c_str(), addr) != 0) {
        log_debug("/af/internet",
                  "admin host '%s' not a valid hostname", url.host_.c_str());
        return false;
    }

    *port = url.port_;

    return true;
}

/**
 * Determine if the administrative string is valid.
 */
bool
InternetAddressFamily::validate(const std::string& admin)
{
    // XXX/demmer fix this
    return true;
}

/**
 * Compare two admin strings, implementing any wildcarding or
 * pattern matching semantics.
 */
bool
InternetAddressFamily::match(const std::string& pattern,
                             const std::string& admin)
{
    size_t patternlen = pattern.length();

    // first of all, try exact matching
    if (pattern.compare(admin) == 0)
        return true;
    
    // otherwise, try supporting * as a trailing character
    if (patternlen >= 1 && pattern[patternlen-1] == '*') {
        patternlen --;
        
        if (pattern.substr(0, patternlen) == admin.substr(0, patternlen))
            return true;
    }

    // XXX/demmer also support CIDR style matching
    
    return false;

}

} // namespace dtn

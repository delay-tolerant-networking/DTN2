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
#include <oasys/util/URL.h>

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
    // make sure it's a valid url
    oasys::URL url(admin);
    return url.valid();
}

/**
 * Compare two admin strings, implementing any wildcarding or
 * pattern matching semantics.
 */
bool
InternetAddressFamily::match(const std::string& pattern,
                             const std::string& admin)
{
    // parse the two strings into URLs for easier manipulation
    oasys::URL pattern_url(pattern);
    oasys::URL admin_url(admin);

    if (!pattern_url.valid()) {
        log_warn("/af/internet", "match error: pattern '%s' not a valid url",
                 pattern.c_str());
        return false;
    }

    if (!admin_url.valid()) {
        log_warn("/af/internet", "match error: admin '%s' not a valid url",
                 pattern.c_str());
        return false;
    }
    
    // check for a wildcard host specifier
    // e.g bundles://internet/host://*
    if (pattern_url.host_ == "*" && pattern_url.path_ == "")
    {
        return true;
    }

    // match the host part of the urls (allowing for a pattern of *)
    if ((pattern_url.host_ != "*") &&
        (pattern_url.host_ != admin_url.host_))
    {
        log_debug("/af/internet",
                  "match failed: url hosts not equal ('%s' != '%s')",
                  pattern_url.host_.c_str(), admin_url.host_.c_str());
        return false;
    }

    // make sure the ports are equal (or unspecified)
    if (pattern_url.port_ != admin_url.port_)
    {
        log_debug("/af/internet",
                  "match failed: url ports not equal (%d != %d)",
                  pattern_url.port_, admin_url.port_);
        return false;
    }

    // check for a wildcard path or an exact match of the path strings
    if ((pattern_url.path_ == "*") ||
        (pattern_url.path_ == admin_url.path_))
    {
        return true;
    }

    // finally, try supporting a trailing * to truncate the path match
    size_t patternlen = pattern_url.path_.length();
    if (patternlen >= 1 && pattern_url.path_[patternlen-1] == '*') {
        patternlen--;
        
        if (pattern_url.path_.substr(0, patternlen) ==
            admin_url.path_.substr(0, patternlen)) {
            return true;
        }
    }

    // XXX/demmer TODO: support CIDR style matching for ip:// addresses
    
    return false;
}

} // namespace dtn

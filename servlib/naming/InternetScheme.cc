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

#include <ctype.h>
#include <oasys/io/IPSocket.h>
#include <oasys/io/NetUtils.h>
#include <oasys/util/URL.h>

#include "InternetScheme.h"
#include "EndpointID.h"

namespace dtn {

template <>
InternetScheme* oasys::Singleton<InternetScheme>::instance_ = 0;

/**
 * Parse out an IP address, port, and application tag from the
 * given ssp. Note that this potentially does a DNS lookup if the
 * string doesn't contain an explicit IP address. Also, if the url
 * did not contain a port, 0 is returned.
 *
 * @return true if the extraction was a success, false if
 * malformed
 */
bool
InternetScheme::parse(const std::string& ssp,
                      in_addr_t* addr, u_int16_t* port, std::string* tag)
{
    // use the oasys builtin class for URLs, though we need to re-add
    // the : since it was stripped by the basic endpoint id parsing
    std::string url_str = "dtn:";
    url_str.append(ssp);
    oasys::URL url(url_str);
    if (! url.valid()) {
        log_debug("/scheme/internet",
                  "ssp '%s' not valid for the internet scheme", ssp.c_str());
        return false;
    }

    // look up the hostname from the url
    *addr = INADDR_NONE;
    if (oasys::gethostbyname(url.host_.c_str(), addr) != 0) {
        log_debug("/scheme/internet",
                  "ssp '%s' hostname component '%s' is not valid",
                  ssp.c_str(), url.host_.c_str());
        return false;
    }

    // copy out the port and tag
    *port = url.port_;
    *tag  = url.path_;

    return true;
}

/**
 * Validate that the given ssp is legitimate for this scheme. If
 * the 'is_pattern' parameter is true, then the ssp is being
 * validated as an EndpointIDPattern.
 *
 * @return true if valid
 */
bool
InternetScheme::validate(const std::string& ssp, bool is_pattern)
{
    // use the oasys builtin class for URLs, though we need to re-add
    // the : since it was stripped by the basic endpoint id parsing
    std::string url_str = "dtn:";
    url_str.append(ssp);
    oasys::URL url(url_str);
    if (! url.valid()) {
        return false;
    }

    // validate that the hostname contains only legal chars.
    // XXX/demmer we could be better about making sure it's really a
    // legal hostname, i.e. doesn't contain two dots in a row,
    // something like like www.foo..com 
    std::string::iterator iter;
    for (iter = url.host_.begin(); iter != url.host_.end(); ++iter) {
        char c = *iter;
        
        if (isalnum(c) || (c == '_') || (c == '-') || (c == '.'))
            continue;

        if (is_pattern && (c == '*'))
            continue;

        log_debug("/scheme/internet",
                  "ssp '%s' contains invalid hostname character '%c'",
                  ssp.c_str(), c);

        return false;
    }
    
    return true;
}

/**
 * Match the given ssp with the given pattern.
 *
 * @return true if it matches
 */
bool
InternetScheme::match(EndpointIDPattern* pattern, const std::string& ssp)
{
    // sanity check
    ASSERT(pattern->scheme() == this);
    
    // parse the two strings into URLs for easier manipulation
    std::string url_str = "dtn:";
    url_str.append(ssp);
    oasys::URL ssp_url(url_str);
    oasys::URL pattern_url(pattern->str());
    
    if (!ssp_url.valid()) {
        log_warn("/scheme/internet",
                 "match error: ssp '%s' not a valid ssp",
                 ssp.c_str());
        return false;
    }
    
    if (!pattern_url.valid()) {
        log_warn("/scheme/internet",
                 "match error: pattern '%s' not a valid url",
                 pattern->c_str());
        return false;
    }
    
    // check for a wildcard host specifier e.g bp0://*
    if (pattern_url.host_ == "*" && pattern_url.path_ == "")
    {
        return true;
    }
    
    // match the host part of the urls (though if the pattern host is
    // "*", fall through to the rest of the comparison)
    if ((pattern_url.host_ != "*") &&
        (pattern_url.host_ != ssp_url.host_))
    {
        log_debug("/scheme/internet",
                  "match failed: url hosts not equal ('%s' != '%s')",
                  pattern_url.host_.c_str(), ssp_url.host_.c_str());
        return false;
    }

    // make sure the ports are equal (or unspecified in which case they're 0)
    if (pattern_url.port_ != ssp_url.port_)
    {
        log_debug("/scheme/internet",
                  "match failed: url ports not equal (%d != %d)",
                  pattern_url.port_, ssp_url.port_);
        return false;
    }

    // check for a wildcard path or an exact match of the path strings
    if ((pattern_url.path_ == "*") ||
        (pattern_url.path_ == ssp_url.path_))
    {
        log_debug("/scheme/internet",
                  "match succeeded: pattern '%s' ssp '%s'",
                  pattern_url.host_.c_str(), ssp_url.host_.c_str());
        return true;
    }

    // finally, try supporting a trailing * to truncate the path match
    size_t patternlen = pattern_url.path_.length();
    if (patternlen >= 1 && pattern_url.path_[patternlen-1] == '*') {
        patternlen--;
        
        if (pattern_url.path_.substr(0, patternlen) ==
            ssp_url.path_.substr(0, patternlen))
        {
            log_debug("/scheme/internet",
                      "match substring succeeded: pattern '%s' ssp '%s'",
                      pattern_url.host_.c_str(), ssp_url.host_.c_str());
            return true;
        }
    }
    
    // XXX/demmer TODO: support CIDR style matching for explicit
    // dotted-quad ip addresses
    
    return false;
}

} // namespace dtn

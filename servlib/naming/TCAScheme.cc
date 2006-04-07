/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * University of Waterloo Open Source License
 * 
 * Copyright (c) 2005 University of Waterloo. All rights reserved. 
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
 *   Neither the name of the University of Waterloo nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY
 * OF WATERLOO OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <ctype.h>
#include <oasys/debug/Log.h>
#include <oasys/util/URL.h>

#include "TCAScheme.h"
#include "EndpointID.h"

namespace dtn {

template <>
TCAScheme* oasys::Singleton<TCAScheme>::instance_ = 0;

/**
 * Validate that the given ssp is legitimate for this scheme. If
 * the 'is_pattern' parameter is true, then the ssp is being
 * validated as an EndpointIDPattern.
 *
 * @return true if valid
 */
bool
TCAScheme::validate(const std::string& ssp, bool is_pattern)
{
    // DK: This is entirely borrowed from the DTNScheme implementation.
    // TODO: Revisit this once we are using email addresses or sha1 hashes
    // of email addresses as guids in the scheme. Are these still valid?

    // use the oasys builtin class for URLs, though we need to re-add
    // the dtn: since it was stripped by the basic endpoint id parsing
    std::string url_str = "tca:";
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

        log_debug("/scheme/tca",
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
// DK: This is almost entirely copied from the DTNScheme implementation.
// The one exception is a change to the matching logic, to allow for wildcard
// * in either pattern or ssp. (Previously, * was only supported in pattern.)
bool
TCAScheme::match(const EndpointIDPattern& pattern, const EndpointID& eid)
{
    // sanity check
    ASSERT(pattern.scheme() == this);

    // we only match endpoint ids of the same scheme
    if (!eid.known_scheme() || (eid.scheme() != this)) {
        return false;
    }

    // if the ssp of either string is "none", then nothing should
    // match it (ever)
    if (pattern.ssp() == "none" || eid.ssp() == "none") {
        return false;
    }

    // parse the two strings into URLs for easier manipulation
    oasys::URL eid_url(eid.str());
    oasys::URL pattern_url(pattern.str());

    if (!eid_url.valid()) {
        log_warn("/scheme/dtn",
                 "match error: eid '%s' not a valid url",
                 eid.c_str());
        return false;
    }

    if (!pattern_url.valid()) {
        log_warn("/scheme/dtn",
                 "match error: pattern '%s' not a valid url",
                 pattern.c_str());
        return false;
    }

    // check for a wildcard host specifier e.g dtn://*
    if (pattern_url.host_ == "*" && pattern_url.path_ == "")
    {
        return true;
    }

    // match the host part of the urls (though if the pattern host is
    // "*", fall through to the rest of the comparison)
    if ((pattern_url.host_ != "*") &&
         (eid_url.host_ != "*") &&                  // DK: added this
         (pattern_url.host_ != eid_url.host_))
    {
        log_debug("/scheme/dtn",
                  "match(%s, %s) failed: url hosts not equal ('%s' != '%s')",
                  eid_url.c_str(), pattern_url.c_str(),
                  pattern_url.host_.c_str(), eid_url.host_.c_str());
        return false;
    }

    // make sure the ports are equal (or unspecified in which case they're 0)
    if (pattern_url.port_ != eid_url.port_)
    {
        log_debug("/scheme/dtn",
                  "match(%s, %s) failed: url ports not equal (%d != %d)",
                  eid_url.c_str(), pattern_url.c_str(),
                  pattern_url.port_, eid_url.port_);
        return false;
    }

    // check for a wildcard path or an exact match of the path strings
    if ((pattern_url.path_ == "*") ||
        (pattern_url.path_ == eid_url.path_))
    {
        log_debug("/scheme/dtn",
                  "match(%s, %s) succeeded: pattern '%s' ssp '%s'",
                  eid_url.c_str(), pattern_url.c_str(),
                  pattern_url.host_.c_str(), eid_url.host_.c_str());
        return true;
    }

    // finally, try supporting a trailing * to truncate the path match
    size_t patternlen = pattern_url.path_.length();
    if (patternlen >= 1 && pattern_url.path_[patternlen-1] == '*') {
        patternlen--;

        if (pattern_url.path_.substr(0, patternlen) ==
            eid_url.path_.substr(0, patternlen))
        {
            log_debug("/scheme/dtn",
                      "match(%s, %s) substring succeeded: "
                      "pattern '%s' ssp '%s'",
                      eid_url.c_str(), pattern_url.c_str(),
                      pattern_url.host_.c_str(), eid_url.host_.c_str());
            return true;
        }
    }

    // XXX/demmer TODO: support CIDR style matching for explicit
    // dotted-quad ip addresses

    return false;
}


/**
 * Append the given service tag to the ssp in a scheme-specific
 * manner.
 *
 * @return true if this scheme is capable of service tags and the
 * tag is a legal one, false otherwise.
 */
bool
TCAScheme::append_service_tag(std::string* ssp, const char* tag)
{
    if (tag[0] != '/') {
        ssp->push_back('/');
    }
    ssp->append(tag);
    return true;
}

} // namespace dtn

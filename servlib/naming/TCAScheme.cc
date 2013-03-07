/*
 *    Copyright 2005-2006 University of Waterloo
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <ctype.h>
#include <oasys/debug/Log.h>

#include "TCAScheme.h"
#include "EndpointID.h"

template <>
dtn::TCAScheme* oasys::Singleton<dtn::TCAScheme>::instance_ = 0;

namespace dtn {

/**
 * Validate that the SSP in the given URI is legitimate for
 * this scheme. If the 'is_pattern' parameter is true, then
 * the ssp is being validated as an EndpointIDPattern.
 *
 * @return true if valid
 */
bool
TCAScheme::validate(const URI& uri, bool is_pattern)
{
    // DK: This is entirely borrowed from the DTNScheme implementation.
    // TODO: Revisit this once we are using email addresses or sha1 hashes
    // of email addresses as guids in the scheme. Are these still valid?

    (void)is_pattern;

    if (!uri.valid()) {
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

    // check for a wildcard host specifier e.g dtn://*
    if (pattern.uri().host() == "*" && pattern.uri().path() == "")
    {
        return true;
    }

    // match the host part of the urls (though if the pattern host is
    // "*", fall through to the rest of the comparison)
    if ((pattern.uri().host() != "*") &&
         (eid.uri().host() != "*") &&                  // DK: added this
         (pattern.uri().host() != eid.uri().host()))
    {
        log_debug_p("/scheme/dtn",
                    "match(%s, %s) failed: url hosts not equal ('%s' != '%s')",
                    eid.uri().c_str(), pattern.uri().c_str(),
                    pattern.uri().host().c_str(), eid.uri().host().c_str());
        return false;
    }

    // make sure the ports are equal (or unspecified in which case they're 0)
    if (pattern.uri().port_num() != eid.uri().port_num())
    {
        log_debug_p("/scheme/dtn",
                    "match(%s, %s) failed: url ports not equal (%d != %d)",
                    eid.uri().c_str(), pattern.uri().c_str(),
                    pattern.uri().port_num(), eid.uri().port_num());
        return false;
    }

    // check for a wildcard path or an exact match of the path strings
    if ((pattern.uri().path() == "*") ||
        (pattern.uri().path() == eid.uri().path()))
    {
        log_debug_p("/scheme/dtn",
                    "match(%s, %s) succeeded: pattern '%s' ssp '%s'",
                    eid.uri().c_str(), pattern.uri().c_str(),
                    pattern.uri().host().c_str(), eid.uri().host().c_str());
        return true;
    }

    // finally, try supporting a trailing * to truncate the path match
    size_t patternlen = pattern.uri().path().length();
    if (patternlen >= 1 && pattern.uri().path()[patternlen-1] == '*') {
        patternlen--;

        if (pattern.uri().path().substr(0, patternlen) ==
            eid.uri().path().substr(0, patternlen))
        {
            log_debug_p("/scheme/dtn",
                        "match(%s, %s) substring succeeded: "
                        "pattern '%s' ssp '%s'",
                        eid.uri().c_str(), pattern.uri().c_str(),
                        pattern.uri().host().c_str(), eid.uri().host().c_str());
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
TCAScheme::append_service_tag(URI* uri, const char* tag)
{
    if (tag[0] != '/') {
        uri->set_path(std::string("/") + tag);
    } else {
        uri->set_path(tag);
    }
    return true;
}

//----------------------------------------------------------------------
Scheme::singleton_info_t
TCAScheme::is_singleton(const URI& uri)
{
    // if there's a * in the hostname part of the URI, then it's not a
    // singleton endpoint
    if (uri.host().find('*') != std::string::npos) {
        return EndpointID::MULTINODE;
    }
    
    return EndpointID::SINGLETON;
}

} // namespace dtn

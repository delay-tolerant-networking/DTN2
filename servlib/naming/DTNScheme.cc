/*
 *    Copyright 2004-2006 Intel Corporation
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


#include <ctype.h>
#include <oasys/debug/Log.h>
#include <oasys/util/Glob.h>
#include <oasys/util/URL.h>

#include "DTNScheme.h"
#include "EndpointID.h"

namespace dtn {

template <>
DTNScheme* oasys::Singleton<DTNScheme>::instance_ = 0;

//----------------------------------------------------------------------
bool
DTNScheme::validate(const std::string& ssp, bool is_pattern)
{
    (void)is_pattern;
    
    // first check for the special ssp that is simply "none"
    if (ssp == "none") {
        return true;
    }
    
    // use the oasys builtin class for URLs, though we need to re-add
    // the dtn scheme since it was stripped by the basic endpoint id
    // parsing
    oasys::URL url("dtn", ssp);
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
        
        if (isalnum(c) || (c == '_') || (c == '-') || (c == '.') || (c == '*'))
            continue;
        
        log_debug_p("/dtn/scheme/dtn",
                    "ssp '%s' contains invalid hostname character '%c'",
                    ssp.c_str(), c);

        return false;
    }
    
    return true;
}

//----------------------------------------------------------------------
bool
DTNScheme::match(const EndpointIDPattern& pattern, const EndpointID& eid)
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
        log_warn_p("/dtn/scheme/dtn",
                   "match error: eid '%s' not a valid url",
                   eid.c_str());
        return false;
    }
    
    if (!pattern_url.valid()) {
        log_warn_p("/dtn/scheme/dtn",
                   "match error: pattern '%s' not a valid url",
                   pattern.c_str());
        return false;
    }

    // check for a wildcard host specifier e.g dtn://*
    if (pattern_url.host_ == "*" && pattern_url.path_ == "")
    {
        return true;
    }
    
    // use glob rules to match the hostname part -- note that we don't
    // distinguish between which one has the glob patterns so we run
    // glob in both directions looking for a match
    if (! (oasys::Glob::fixed_glob(pattern_url.host_.c_str(),
                                   eid_url.host_.c_str()) ||
           oasys::Glob::fixed_glob(eid_url.host_.c_str(),
                                   pattern_url.host_.c_str())) )
    {
        log_debug_p("/dtn/scheme/dtn",
                    "match(%s, %s) failed: url hosts don't glob ('%s' != '%s')",
                    eid_url.c_str(), pattern_url.c_str(),
                    pattern_url.host_.c_str(), eid_url.host_.c_str());
        return false;
    }

    // make sure the ports are equal (or unspecified in which case they're 0)
    if (pattern_url.port_ != eid_url.port_)
    {
        log_debug_p("/dtn/scheme/dtn",
                    "match(%s, %s) failed: url ports not equal (%d != %d)",
                    eid_url.c_str(), pattern_url.c_str(),
                    pattern_url.port_, eid_url.port_);
        return false;
    }

    // check for a glob match of the path strings
    if (! (oasys::Glob::fixed_glob(pattern_url.path_.c_str(),
                                   eid_url.path_.c_str()) ||
           oasys::Glob::fixed_glob(eid_url.path_.c_str(),
                                   pattern_url.path_.c_str())) )
    {
        log_debug_p("/dtn/scheme/dtn",
                    "match(%s, %s) failed: paths don't glob ('%s' != '%s')",
                    eid_url.c_str(), pattern_url.c_str(),
                    pattern_url.path_.c_str(), eid_url.path_.c_str());
        return false;
    }

    // finally, check for an exact match of the url parameters
    if (! (pattern_url.params_ == eid_url.params_))
    {
        log_debug_p("/dtn/scheme/dtn",
                    "match(%s, %s) failed: parameter mismatch",
                    eid_url.c_str(), pattern_url.c_str());
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
bool
DTNScheme::append_service_tag(std::string* ssp, const char* tag)
{
    
    if (tag[0] != '/') {
        ssp->push_back('/');
    }
    ssp->append(tag);
    return true;
}

} // namespace dtn

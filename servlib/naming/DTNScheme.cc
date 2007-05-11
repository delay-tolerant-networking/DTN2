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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <ctype.h>
#include <oasys/debug/Log.h>
#include <oasys/util/Glob.h>

#include "DTNScheme.h"
#include "EndpointID.h"

namespace dtn {

template <>
DTNScheme* oasys::Singleton<DTNScheme>::instance_ = 0;

//----------------------------------------------------------------------
bool
DTNScheme::validate(const URI& uri, bool is_pattern)
{
    (void)is_pattern;

    if (!uri.valid()) {
        log_debug_p("/dtn/scheme/dtn", "DTNScheme::validate: invalid URI");
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
    
    if (!eid.uri().valid()) {
        log_warn_p("/dtn/scheme/dtn",
                   "match error: eid '%s' not a valid uri",
                   eid.c_str());
        return false;
    }
    
    if (!pattern.uri().valid()) {
        log_warn_p("/dtn/scheme/dtn",
                   "match error: pattern '%s' not a valid uri",
                   pattern.c_str());
        return false;
    }

    // check for a wildcard host specifier e.g dtn://*
    if (pattern.uri().host() == "*" && pattern.uri().path() == "")
    {
        return true;
    }
    
    // use glob rules to match the hostname part -- note that we don't
    // distinguish between which one has the glob patterns so we run
    // glob in both directions looking for a match
    if (! (oasys::Glob::fixed_glob(pattern.uri().host().c_str(),
                                   eid.uri().host().c_str()) ||
           oasys::Glob::fixed_glob(eid.uri().host().c_str(),
                                   pattern.uri().host().c_str())) )
    {
        log_debug_p("/dtn/scheme/dtn",
                    "match(%s, %s) failed: uri hosts don't glob ('%s' != '%s')",
                    eid.uri().c_str(), pattern.uri().c_str(),
                    pattern.uri().host().c_str(), eid.uri().host().c_str());
        return false;
    }

    // make sure the ports are equal (or unspecified in which case they're 0)
    if (pattern.uri().port_num() != eid.uri().port_num())
    {
        log_debug_p("/dtn/scheme/dtn",
                    "match(%s, %s) failed: uri ports not equal (%d != %d)",
                    eid.uri().c_str(), pattern.uri().c_str(),
                    pattern.uri().port_num(), eid.uri().port_num());
        return false;
    }

    std::string pattern_path(pattern.uri().path());
    if ((pattern_path.length() >= 2) &&
        (pattern_path.substr((pattern_path.length() - 2), 2) == "/*")) {
        pattern_path.replace((pattern_path.length() - 2), 2, 1, '*');
    }

    // check for a glob match of the path strings
    if (! (oasys::Glob::fixed_glob(pattern_path.c_str(),
                                   eid.uri().path().c_str()) ||
           oasys::Glob::fixed_glob(eid.uri().path().c_str(),
                                   pattern_path.c_str())) )
    {
        log_debug_p("/dtn/scheme/dtn",
                    "match(%s, %s) failed: paths don't glob ('%s' != '%s')",
                    eid.uri().c_str(), pattern.uri().c_str(),
                    pattern.uri().path().c_str(), eid.uri().path().c_str());
        return false;
    }

    // ignore the query parameter strings, so they still match the glob'd routes
    return true;
}

//----------------------------------------------------------------------
bool
DTNScheme::append_service_tag(URI* uri, const char* tag)
{
    // XXX/demmer this would be better if it didn't use the ssp_ptr()
    // but instead manipulated just the path part of the URI. This
    // implementation won't work properly if the URI has query
    // parameters, etc. following the path.
    
    if (tag[0] != '/') {
        uri->ssp_ptr()->push_back('/');
    }
    uri->ssp_ptr()->append(tag);
    return true;
}

//----------------------------------------------------------------------
bool
DTNScheme::append_service_wildcard(URI* uri)
{
    if (uri == NULL) return false;

    // only append wildcard if path is empty
    if (! uri->path().empty())
        return false;

    uri->ssp_ptr()->append("/*");
    return true;
}

//----------------------------------------------------------------------
bool
DTNScheme::remove_service_tag(URI* uri)
{
    if (uri == NULL) return false;
    uri->assign(uri->scheme() + ':' + uri->authority());
    return true;
}

//----------------------------------------------------------------------
bool
DTNScheme::is_singleton(const URI& uri)
{
    // if there's a * in the hostname part of the URI, then it's not a
    // singleton endpoint
    if (uri.host().find('*') != std::string::npos) {
        return false;
    }
    
    return true;
}

} // namespace dtn

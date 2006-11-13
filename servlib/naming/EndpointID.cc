/*
 *    Copyright 2005-2006 Intel Corporation
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

#include "applib/dtn_types.h"
#include "EndpointID.h"
#include "Scheme.h"
#include "SchemeTable.h"

namespace dtn {

/**
 * Extract and look up the scheme and ssp.
 *
 * @return true if the string is a valid endpoint id, false if not.
 */
bool
EndpointID::parse()
{
    size_t pos;

    scheme_ = NULL;
    scheme_str_.erase();
    ssp_.erase();
    valid_ = false;
    
    if ((pos = str_.find(':')) == std::string::npos)
        return false; // no :

    if (pos == 0) {
        return false; // empty scheme
    }
    
    scheme_str_.assign(str_, 0, pos);

    if (!is_pattern_) {
        // validate that scheme is composed of legitimate characters
        // according to the URI spec (RFC 3986)
        std::string::iterator iter = scheme_str_.begin();
        if (! isalpha(*iter)) {
            return false;
        }
        ++iter;
        for (; iter != scheme_str_.end(); ++iter) {
            char c = *iter;
            if (isalnum(c) || (c == '+') || (c == '-') || (c == '.'))
                continue;
        
            return false; // invalid character
        }
    }

    // XXX/demmer should really validate the rest of it, but the URI
    // validation rules are actually a bit complex...
    scheme_ = SchemeTable::instance()->lookup(scheme_str_);
    
    if (pos == str_.length() - 1) {
        return false; // empty scheme or ssp
    }
    ssp_.assign(str_, pos + 1, str_.length() - pos);


    if (scheme_) {
        valid_ = scheme_->validate(ssp_, is_pattern_);
    } else {
        valid_ = true;
    }
    
    return valid_;
}

/**
 * Append the specified service tag (in a scheme-specific manner)
 * to the ssp.
 *
 * @return true if successful, false if the scheme doesn't support
 * service tags
 */
bool
EndpointID::append_service_tag(const char* tag)
{
    if (!scheme_)
        return false;

    bool ok = scheme_->append_service_tag(&ssp_, tag);
    if (!ok)
        return false;

    // rebuild the string
    str_ = scheme_str_ + ":" + ssp_;
    
    return true;
}

/**
 * Set the string from the API type dtn_endpoint_id_t
 *
 * @return true if the string is a valid id, false if not.
 */
bool
EndpointID::assign(const dtn_endpoint_id_t* eid)
{
    str_.assign(eid->uri);
    return parse();
}
    
/**
 * Copy the endpoint id contents out to the API type
 * dtn_endpoint_id_t.
 */
void
EndpointID::copyto(dtn_endpoint_id_t* eid) const
{
    ASSERT(str_.length() <= DTN_MAX_ENDPOINT_ID + 1);
    strcpy(eid->uri, str_.c_str());
}


/**
 * Virtual from SerializableObject
 */
void
EndpointID::serialize(oasys::SerializeAction* a)
{
    a->process("uri", &str_);
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        parse();
    }
}


/**
 * Shortcut to the matching functionality implemented by the
 * scheme.
 */
bool
EndpointIDPattern::match(const EndpointID& eid) const
{
    if (! known_scheme()) {
        return false;
    }

    return scheme()->match(*this, eid);
}


} // namespace dtn

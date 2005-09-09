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
    
    return true;
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
        ASSERT(valid_);
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

    if (scheme() != eid.scheme()) {
        return false;
    }
    
    return scheme()->match(this, eid.ssp());
}


} // namespace dtn

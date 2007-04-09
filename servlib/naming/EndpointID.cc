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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <ctype.h>

#include <oasys/debug/Log.h>

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
    static const char* log = "/dtn/naming/endpoint/";

    scheme_ = NULL;
    valid_ = false;

    uri_.parse();
    if (!uri_.valid()) {
        log_debug_p(log, "EndpointID::parse: invalid URI");
        return false;
    }
    
    if (scheme_str().length() > MAX_EID_PART_LENGTH) {
        log_err_p(log, "scheme name is too large (>%zu)", MAX_EID_PART_LENGTH);
        valid_ = false;
        return false;
    }
    
    if (ssp().length() > MAX_EID_PART_LENGTH) {
        log_err_p(log, "ssp is too large (>%zu)", MAX_EID_PART_LENGTH);
        valid_ = false;
        return false;
    }

    valid_ = true;

    if ((scheme_ = SchemeTable::instance()->lookup(uri_.scheme())) != NULL) {
        valid_ = scheme_->validate(uri_, is_pattern_);
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

    bool ok = scheme_->append_service_tag(&uri_, tag);
    if (!ok)
        return false;

    // rebuild the string
    uri_.format();
    if (!uri_.valid()) {
        log_err_p("/dtn/naming/endpoint/",
                  "EndpointID::append_service_tag: "
                  "failed to format appended URI");
        
        // XXX/demmer this leaves the URI in a bogus state... that
        // doesn't seem good since this really shouldn't ever happen
        return false;
    }

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
    uri_.assign(std::string(eid->uri));
    return parse();
}
    
/**
 * Copy the endpoint id contents out to the API type
 * dtn_endpoint_id_t.
 */
void
EndpointID::copyto(dtn_endpoint_id_t* eid) const
{
    ASSERT(uri_.uri().length() <= DTN_MAX_ENDPOINT_ID + 1);
    strcpy(eid->uri, uri_.uri().c_str());
}


/**
 * Virtual from SerializableObject
 */
void
EndpointID::serialize(oasys::SerializeAction* a)
{
    uri_.serialize(a);
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

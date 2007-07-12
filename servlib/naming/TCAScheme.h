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

#ifndef _TCA_SCHEME_H_
#define _TCA_SCHEME_H_

#include "Scheme.h"
#include <oasys/util/Singleton.h>

namespace dtn {


/**
 * This class implements the tca scheme.
 * SSPs for this scheme take the canonical form:
 *
 * <code>
 * tca://<router identifier (guid)>[/<application tag>]
 * </code>
 *
 * Where "router identifier" is a globally unique identifier.
 * In practice, this will often be the DNS-style "hostname" string of
 * an internet host, for the more-or-less static "infrastructure" nodes
 * that make up the routers and gateways of the TCA network.
 * For the true TCA mobiles, it may be something quite different, like
 * the sha1 hash of an email address for example.
 *
 * "application tag" is any string of URI-valid characters.
 *
 * This implementation also supports limited wildcard matching for
 * endpoint patterns.
 */
class TCAScheme : public Scheme, public oasys::Singleton<TCAScheme> {
public:
    /**
     * Validate that the SSP in the given URI is legitimate for
     * this scheme. If the 'is_pattern' paraemeter is true, then
     * the ssp is being validated as an EndpointIDPattern.
     *
     * @return true if valid
     */
    virtual bool validate(const URI& uri, bool is_pattern = false);

    /**
     * Match the pattern to the endpoint id in a scheme-specific
     * manner.
     */
    virtual bool match(const EndpointIDPattern& pattern,
                       const EndpointID& eid);

    /**
     * Append the given service tag to the URI in a scheme-specific
     * manner.
     *
     * @return true if this scheme is capable of service tags and the
     * tag is a legal one, false otherwise.
     */
    virtual bool append_service_tag(URI* uri, const char* tag);
    
    /**
     * Check if the given URI is a singleton EID.
     */
    virtual singleton_info_t is_singleton(const URI& uri);
    
private:
    friend class oasys::Singleton<TCAScheme>;
    TCAScheme() {}
};
    
}

#endif /* _TCA_SCHEME_H_ */

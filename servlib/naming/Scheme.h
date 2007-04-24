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

#ifndef _SCHEME_H_
#define _SCHEME_H_

#include <string>

#include <oasys/util/URI.h>

namespace dtn {

typedef oasys::URI URI;

class EndpointID;
class EndpointIDPattern;

/**
 * The base class for various endpoint id schemes. The class provides
 * two pure virtual methods -- validate() and match() -- that are
 * overridden by the various scheme implementations.
 */
class Scheme {
public:
    /**
     * Destructor -- should be called only at shutdown time.
     */
    virtual ~Scheme();

    /**
     * Validate that the SSP within the given URI is legitimate for
     * this scheme. If the 'is_pattern' paraemeter is true, then the
     * ssp is being validated as an EndpointIDPattern.
     *
     * @return true if valid
     */
    virtual bool validate(const URI& uri, bool is_pattern = false) = 0;

    /**
     * Match the pattern to the endpoint id in a scheme-specific
     * manner.
     */
    virtual bool match(const EndpointIDPattern& pattern,
                       const EndpointID& eid) = 0;
    
    /**
     * Append the given service tag to the uri in a scheme-specific
     * manner. By default, the scheme is not capable of this.
     *
     * @return true if this scheme is capable of service tags and the
     * tag is a legal one, false otherwise.
     */
    virtual bool append_service_tag(URI* uri, const char* tag)
    {
        (void)uri;
        (void)tag;
        return false;
    }

    /**
     * Check if the given URI is a singleton endpoint id.
     */
    virtual bool is_singleton(const URI& uri)
    {
        (void)uri;
        return true;
    }
};
    
}

#endif /* _SCHEME_H_ */

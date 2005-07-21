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
#ifndef _ENDPOINT_ID_H_
#define _ENDPOINT_ID_H_

#include <string>

namespace dtn {

class EndpointID;
class EndpointIDPattern;
class Scheme;

class EndpointID {
public:
    /**
     * Default constructor
     */
    EndpointID() : scheme_(NULL), valid_(false), is_pattern_(false) {}

    /**
     * Construct the endpoint id from the given string.
     */
    EndpointID(const std::string& str)
        : str_(str), scheme_(NULL), valid_(false), is_pattern_(false)
    {
        parse();
    }

    /**
     * Set the string and parse it.
     * @return true if the string is a valid id, false if not.
     */
    bool assign(const std::string& str)
    {
        str_.assign(str);
        return parse();
    }
    
    /**
     * Return an indication of whether or not the scheme is known.
     */
    bool known_scheme() const
    {
        return (scheme_ != NULL);
    }

    /// @{
    /// Accessors and wrappers around the various fields.
    ///
    const std::string& str()        const { return str_; }
    const std::string& scheme_str() const { return scheme_str_; }
    const std::string& ssp()        const { return ssp_; }
    Scheme*            scheme()     const { return scheme_; }
    bool               valid()      const { return valid_; }
    bool               is_pattern() const { return is_pattern_; }
    const char*        c_str()      const { return str_.c_str(); } 
    const char*        data()       const { return str_.data(); }
    size_t             length()     const { return str_.length(); }
    ///@}

protected:
    /**
     * Extract and look up the scheme and ssp.
     *
     * @return true if the string is a valid endpoint id, false if not.
     */
    bool parse();

    std::string str_;		/* the whole string  */
    std::string scheme_str_;	/* the scheme string  */
    std::string ssp_;		/* the scheme-specific part */
    Scheme* scheme_;		/* the scheme class (if known) */
    bool valid_;		/* true iff the endpoint id is valid */
    bool is_pattern_;		/* true iff this is an EndpointIDPattern */
};

/**
 * A Distinct class for endpoint patterns (i.e. those containing some
 * form of wildcarding) as opposed to basic endpoint IDs to help keep
 * it straight in the code.
 */
class EndpointIDPattern : public EndpointID {
public:
    /**
     * Default constructor
     */
    EndpointIDPattern() : EndpointID()
    {
        is_pattern_ = true;
    }

    /**
     * Construct the endpoint id pattern from the given string.
     */
    EndpointIDPattern(const std::string& str) : EndpointID()
    {
        is_pattern_ = true;
        assign(str);
    }

    /**
     * Shortcut to the matching functionality implemented by the
     * scheme.
     */
    bool match(EndpointID* eid);
   
};

} // namespace dtn

#endif /* _ENDPOINT_ID_H_ */

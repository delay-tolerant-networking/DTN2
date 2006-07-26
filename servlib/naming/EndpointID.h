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
#include <oasys/serialize/Serialize.h>
#include <oasys/serialize/SerializableVector.h>

struct dtn_endpoint_id_t;

namespace dtn {

class EndpointID;
class EndpointIDPattern;
class Scheme;

class EndpointID : public oasys::SerializableObject {
public:
    /**
     * Default constructor
     */
    EndpointID() : scheme_(NULL), valid_(false), is_pattern_(false) {}

    /**
     * Constructor for deserialization.
     */
    EndpointID(const oasys::Builder&)
        : scheme_(NULL), valid_(false), is_pattern_(false) {}

    /**
     * Construct the endpoint id from the given string.
     */
    EndpointID(const std::string& str)
        : str_(str), scheme_(NULL), valid_(false), is_pattern_(false)
    {
        parse();
    }

    /**
     * Construct the endpoint id from another.
     */
    EndpointID(const EndpointID& other)
        : SerializableObject(other)
    {
	if (&other != this)
	    assign(other);
    }

    /**
     * Destructor.
     */
    virtual ~EndpointID() {}

    /**
     * Assign this endpoint ID as a copy of the other.
     */
    bool assign(const EndpointID& other)
    {
        str_         = other.str_;
        scheme_str_  = other.scheme_str_;
        ssp_         = other.ssp_;
        scheme_      = other.scheme_;
        valid_       = other.valid_;
        is_pattern_  = other.is_pattern_;
        return true;
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
     * Set the string from component pieces and parse it.
     * @return true if the string is a valid id, false if not.
     */
    bool assign(const std::string& scheme, const std::string& ssp)
    {
        str_ = scheme + ":" + ssp;
        return parse();
    }

    /**
     * Simple equality test function
     */
    bool equals(const EndpointID& other) const
    {
        return str_ == other.str_;
    }

    /**
     * Operator overload for equality syntactic sugar
     */
    bool operator==(const EndpointID& other) const
    {
        return str_ == other.str_;
    }
    
    /**
     * Operator overload for inequality syntactic sugar
     */
    bool operator!=(const EndpointID& other) const
    {
        return str_ != other.str_;
    }

    /**
     * Set the string from the API type dtn_endpoint_id_t
     *
     * @return true if the string is a valid id, false if not.
     */
    bool assign(const dtn_endpoint_id_t* eid);

    /**
     * Append the specified service tag (in a scheme-specific manner)
     * to the ssp.
     *
     * @return true if successful, false if the scheme doesn't support
     * service tags
     */
    bool append_service_tag(const char* tag);

    /**
     * Copy the endpoint id contents out to the API type
     * dtn_endpoint_id_t.
     */
    void copyto(dtn_endpoint_id_t* eid) const;

    /**
     * Return an indication of whether or not the scheme is known.
     */
    bool known_scheme() const
    {
        return (scheme_ != NULL);
    }

    /**
     * Return the special endpoint id used for the null endpoint,
     * namely "dtn:none".
     */
    static const EndpointID NULL_EID() { return EndpointID("dtn:none"); }
    
    /**
     * Return the special wildcard Endpoint ID. This functionality is
     * not in the bundle spec, but is used internally to this
     * implementation.
     */
    static const EndpointID WILDCARD_EID() { return EndpointID("*:*"); }
    
    /**
     * Virtual from SerializableObject
     */
    virtual void serialize(oasys::SerializeAction* a);
    
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
     * Construct the endpoint id pattern from another.
     */
    EndpointIDPattern(const EndpointIDPattern& other)
         : EndpointID(other)
    {
        is_pattern_ = true;
        assign(other);
    }

    /**
     * Construct the endpoint id pattern from another that is not
     * necessarily a pattern.
     */
    EndpointIDPattern(const EndpointID& other)
    {
        is_pattern_ = true;
        assign(other);
    }

    /**
     * Shortcut to the matching functionality implemented by the
     * scheme.
     */
    bool match(const EndpointID& eid) const;
   
};

/**
 * A (serializable) vector of endpoint ids.
 */
class EndpointIDVector : public oasys::SerializableVector<EndpointID> {};

} // namespace dtn

#endif /* _ENDPOINT_ID_H_ */

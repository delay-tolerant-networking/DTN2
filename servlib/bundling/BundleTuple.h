/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
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
#ifndef _BUNDLE_TUPLE_H_
#define _BUNDLE_TUPLE_H_

#include <oasys/serialize/Serialize.h>

struct dtn_tuple_t;

namespace dtn {

class AddressFamily;
class BundleTuple;
class BundleTuplePattern;

/**
 * Class to wrap a bundle tuple of the general form:
 *
 * @code
 * bundles://<region>/<admin>
 * @endcode
 *
 * The region must not contain any '/' characters. and determines a
 * particular convergence layer to use, based on parsing the admin
 * portion.
 */
class BundleTuple : public oasys::SerializableObject {
public:
    BundleTuple();
    BundleTuple(const char* tuple);
    BundleTuple(const std::string& tuple);
    BundleTuple(const BundleTuple& other);
    virtual ~BundleTuple();

    /**
     * Copy the given tuple.
     */
    void assign(const BundleTuple& tuple);

    /**
     * Assignment operator.
     */
    BundleTuple& operator =(const BundleTuple& other)
    {
        assign(other);
        return *this;
    }
    
    /**
     * Parse and assign the given tuple string.
     */
    void assign(const std::string& tuple)
    {
        tuple_.assign(tuple);
        parse_tuple();
    }
    
    /**
     * Parse and assign the given tuple string.
     */
    void assign(const char* str, size_t len = 0)
    {
        if (len == 0) {
            tuple_.assign(str);
        } else {
            tuple_.assign(str, len);
        }
        
        parse_tuple();
    }
    
    /**
     * Parse and assign the given tuple string from it's component
     * parts
     */
    void assign(const std::string& region, const std::string& admin)
    {
        tuple_.assign("bundles://");
        tuple_.append(region);
        if (tuple_[tuple_.length() - 1] != '/') {
            tuple_.push_back('/');
        }
        tuple_.append(admin);
        parse_tuple();
    }
    
    /**
     * Parse and assign the given tuple string.
     */
    void assign(char* region, size_t regionlen,
                char* admin,  size_t adminlen)
    {
        tuple_.assign("bundles://");
        tuple_.append(region, regionlen);
        if (tuple_[tuple_.length() - 1] != '/') {
            tuple_.push_back('/');
        }
        tuple_.append(admin, adminlen);
        parse_tuple();
    }

    /**
     * Parse and assign the given tuple string.
     */
    void assign(u_char* region, size_t regionlen,
                u_char* admin,  size_t adminlen)
    {
        assign((char*)region, regionlen, (char*)admin, adminlen);
    }

    /**
     * Assign the BundleTuple from the dtn_tuple_t.
     */
    void assign(dtn_tuple_t* tuple);

    /**
     * Copy the BundleTuple to the api exposed structure.
     */
    void copyto(dtn_tuple_t* tuple);
    
    // Accessors
    const std::string& tuple()  const { return tuple_; }
    size_t	       length() const { return tuple_.length(); }
    const char*        data()   const { return tuple_.data(); }
    const char*        c_str()  const { return tuple_.c_str(); }
    
    const std::string& region() const { return region_; }
    const std::string& admin()  const { return admin_; }
    AddressFamily*     family() const { return family_; }
    
    /**
     * @return Whether or not the tuple is valid.
     */
    bool valid() const { return valid_; }

    /**
     * Comparison function
     */
    int compare(const BundleTuple& other) const
    {
        return tuple_.compare(other.tuple_);
    }
    
    /**
     * Return true if the two tuples are equal.
     */
    bool equals(const BundleTuple& other) const
    {
        return (compare(other) == 0);
    }

    /**
     * Virtual from SerializableObject
     */
    virtual void serialize(oasys::SerializeAction* a);

protected:
    
    void parse_tuple();
    
    bool valid_;
    AddressFamily* family_;
    std::string tuple_;
    std::string region_;
    std::string admin_;
};

/**
 * A class that implements pattern matching of bundle tuples. The
 * pattern class has the same general form as the basic BundleTuple,
 * and implements a hierarchical wildcard matching on the region and
 * dispatches to the convergence layer to match the admin portion.
 */
class BundleTuplePattern : public BundleTuple {
public:
    BundleTuplePattern() 			 : BundleTuple() {}
    BundleTuplePattern(const char* tuple) 	 : BundleTuple(tuple) {}
    BundleTuplePattern(const std::string& tuple) : BundleTuple(tuple) {}
    BundleTuplePattern(const BundleTuple& tuple) : BundleTuple(tuple) {}

    /**
     * Matching function, implementing wildcarding semantics.
     */
    bool match(const BundleTuple& tuple) const;

protected:
    bool match_region(const std::string& tuple_region) const;
};

} // namespace dtn

#endif /* _BUNDLE_TUPLE_H_ */

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
#ifndef _ADDRESS_FAMILY_H_
#define _ADDRESS_FAMILY_H_

#include <oasys/debug/DebugUtils.h>
#include <oasys/util/StringUtils.h>

namespace dtn {

/**
 * To support different types of addressing within different regions,
 * each region identifier is used as a lookup key into the table of
 * supported address families.
 */
class AddressFamily {
public:
    /**
     * Constructor.
     */
    AddressFamily(std::string& schema);
    AddressFamily(const char* schema);

    /**
     * Destructor (should never be called).
     */
    virtual ~AddressFamily();

    /**
     * Determine if the administrative string is valid. By default
     * this returns true.
     */
    virtual bool validate(const std::string& admin);
    
    /**
     * Compare two admin strings, implementing any wildcarding or
     * pattern matching semantics. The default implementation just
     * does a byte by byte string comparison.
     */
    virtual bool match(const std::string& pattern,
                       const std::string& admin);

    /**
     * Return the schema name for this address family.
     */
    const std::string& schema() const { return schema_; }

protected:
    std::string schema_;	///< the schema for this family
};

/**
 * The table of registered address families.
 */
class AddressFamilyTable {
public:
    /**
     * Singleton accessor.
     */
    static AddressFamilyTable* instance() {
        ASSERT(instance_ != NULL);
        return instance_;
    }

    /**
     * Boot time initializer.
     */
    static void init()
    {
        ASSERT(instance_ == NULL);
        instance_ = new AddressFamilyTable();
    }

    /**
     * Constructor -- instantiates and registers all known address
     * families.
     */
    AddressFamilyTable();

    /**
     * Find the appropriate AddressFamily instance based on the URI
     * schema of the admin string. Then call its appropriate validate
     * function if the caller requests it.
     *
     * @return NULL if there's no match
     */ 
    AddressFamily* lookup(const std::string& admin, bool* validp = NULL);

    /**
     * For integer value admin strings, we use the fixed length
     * address family.
     */
    AddressFamily* fixed_family() { return fixed_family_; }
    
    /**
     * The special wildcard address family.
     */
    AddressFamily* wildcard_family() { return wildcard_family_; }
    
    /**
     * The special string address family.
     */
    AddressFamily* string_family() { return string_family_; }

    /**
     * Initializer call to enable string-based addressing.
     */
    void add_string_family();
    
protected:
    static AddressFamilyTable* instance_;
    
    typedef oasys::StringHashMap<AddressFamily*> AFTable;
    AFTable table_;

    AddressFamily* fixed_family_;
    AddressFamily* wildcard_family_;
    AddressFamily* string_family_;
};

} // namespace dtn

#endif /* _ADDRESS_FAMILY_H_ */

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

#include "AddressFamily.h"
#include "FixedAddressFamily.h"
#include "InternetAddressFamily.h"
#include "EthernetAddressFamily.h"
#include "SmtpAddressFamily.h"
#include "StringAddressFamily.h"
#include "WildcardAddressFamily.h"

namespace dtn {

AddressFamilyTable* AddressFamilyTable::instance_;

/**
 * Constructor.
 */
AddressFamily::AddressFamily(std::string& schema)
    : schema_(schema)
{
}

/**
 * Constructor.
 */
AddressFamily::AddressFamily(const char* schema)
    : schema_(schema)
{
}

/**
 * Destructor (should never be called).
 */
AddressFamily::~AddressFamily()
{
    NOTREACHED;
}

/**
 * Determine if the administrative string is valid. By default
 * this returns true.
 */
bool
AddressFamily::validate(const std::string& admin)
{
    return true;
}
    
/**
 * Compare two admin strings, implementing any wildcarding or
 * pattern matching semantics. The default implementation just
 * does a byte by byte string comparison.
 */
bool
AddressFamily::match(const std::string& pattern,
                     const std::string& admin)
{
    return (pattern.compare(admin) == 0);
}

/**
 * Constructor -- instantiates and registers all known address
 * families.
 */
AddressFamilyTable::AddressFamilyTable()
{
    fixed_family_ 	= new FixedAddressFamily();
    wildcard_family_	= new WildcardAddressFamily();
    string_family_	= NULL;

#ifdef __linux__
    table_["eth"]       = new EthernetAddressFamily("eth");
#endif

    table_["host"] 	= new InternetAddressFamily("host");
    table_["ip"] 	= new InternetAddressFamily("ip");
    table_["smtp"]	= new SmtpAddressFamily();
    table_["string"]	= new StringAddressFamily();
}


/**
 * Initializer call to enable string-based addressing.
 */
void
AddressFamilyTable::add_string_family()
{
    string_family_ 	= new StringAddressFamily();
}

/**
 * Find the appropriate AddressFamily instance based on the URI
 * schema of the admin string.
 *
 * @return the address family or NULL if there's no match.
 */
AddressFamily*
AddressFamilyTable::lookup(const std::string& admin, bool* validp)
{
    AddressFamily* family;

    // check for wildcard admin strings that matches all
    // address families
    if (admin.compare("*") == 0 || admin.compare("*:*") == 0)
    {
        if (validp)
            *validp = true;
        return wildcard_family_;
    }
    
    // check if the admin part is a fixed-length specification
    // (e.g. 0xab or 0xabcd)
    if (admin[0] == '0' && admin[1] == 'x')
    {
        if (validp)
            *validp = fixed_family_->validate(admin);
        return fixed_family_;
    }

    // otherwise, check if the admin string is a URI by trying to
    // extract the schema. 
    size_t end = admin.find(':');
    if (end == std::string::npos) {
        // not a URI
        if (string_family_) {
            if (validp)
                *validp = true;
            return string_family_;
        } else {
            log_debug("/bundle/tuple",
                      "no colon for schema in '%s'", admin.c_str());
            return NULL;
        }
    }
    
    // string is a valid uri, so try to find the address family
    std::string schema(admin, 0, end);
    AFTable::iterator iter;

    iter = table_.find(schema);
    if (iter == table_.end()) {
        log_err("/bundle/tuple", "unknown schema '%s' in admin '%s'",
                schema.c_str(), admin.c_str());
        return NULL;
    }
    
    family = (*iter).second;

    if (validp)
        *validp = family->validate(admin);
    
    return family;
}

} // namespace dtn

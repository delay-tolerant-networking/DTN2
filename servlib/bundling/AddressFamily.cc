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
#include "SmtpAddressFamily.h"
#include "WildcardAddressFamily.h"

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
AddressFamily::valid(const std::string& admin)
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
    
    table_["host"] 	= new InternetAddressFamily("host");
    table_["ip"] 	= new InternetAddressFamily("ip");
    table_["smtp"]	= new SmtpAddressFamily();

    // XXX/demmer temp
    table_["tcp"] 	= new InternetAddressFamily("tcp");
    table_["udp"] 	= new InternetAddressFamily("udp");
    table_["simcl"] 	= new InternetAddressFamily("simcl");
}

/**
 * Find the appropriate AddressFamily instance based on the URI
 * schema of the admin string.
 *
 * @return the address family or NULL if there's no match.
 */
AddressFamily*
AddressFamilyTable::lookup(const std::string& schema)
{
    AFTable::iterator iter;

    iter = table_.find(schema);
    if (iter != table_.end()) {
        return (*iter).second;
    }

    return NULL;
}

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
#ifndef _INTERNET_ADDRESS_FAMILY_H_
#define _INTERNET_ADDRESS_FAMILY_H_

#include "AddressFamily.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <oasys/compat/inttypes.h>

namespace dtn {

/**
 * Address family to represent internet-style addresses.
 *
 * e.g:  host://sandbox.dtnrg.org/demux
 * e.g:  ip://127.0.0.1/demux
 */
class InternetAddressFamily : public AddressFamily {
public:
    InternetAddressFamily(const char* schema)
        : AddressFamily(schema)
    {
    }
    
    /**
     * Determine if the administrative string is valid.
     */
    bool validate(const std::string& admin);
    
    /**
     * Compare two admin strings, implementing any wildcarding or
     * pattern matching semantics.
     */
    bool match(const std::string& pattern,
               const std::string& admin);

    /**
     * Given an admin string , parse out the ip address and port.
     * Potentially does a hostname lookup if the admin string doesn't
     * contain a specified IP address. If the url did not contain a
     * port, 0 is returned.
     *
     * @return true if the extraction was a success, false if the url
     * is malformed or is not in the internet address family.
     */
    static bool parse(const std::string& admin,
                      in_addr_t* addr, u_int16_t* port);
     
};

} // namespace dtn

#endif /* _INTERNET_ADDRESS_FAMILY_H_ */

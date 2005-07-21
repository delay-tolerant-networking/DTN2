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
#ifndef _INTERNET_SCHEME_H_
#define _INTERNET_SCHEME_H_

#include "Scheme.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <oasys/compat/inttypes.h>
#include <oasys/util/Singleton.h>

namespace dtn {


/**
 * Class for internet-style endpoint ids. This class implements the
 * one default scheme as specified in the bundle protocol. SSPs for
 * this scheme take the canonical form:
 *
 * <scheme>://<host identifier>[:<port number>][/<application tag>]
 *
 * Where <host identifier> can be either a dotted-quad IP address or a
 * DNS name, <port number> is a number in the range 0-65535 and
 * <application tag> is any ASCII string.
 *
 * This implementation also supports limited wildcard matching for
 * endpoint patterns.
 */
class InternetScheme : public Scheme, public oasys::Singleton<InternetScheme> {
public:
    /**
     * Validate that the given ssp is legitimate for this scheme. If
     * the 'is_pattern' paraemeter is true, then the ssp is being
     * validated as an EndpointIDPattern.
     *
     * @return true if valid
     */
    virtual bool validate(const std::string& ssp, bool is_pattern = false);

    /**
     * Match the given ssp with the given pattern.
     *
     * @return true if it matches
     */
    virtual bool match(EndpointIDPattern* pattern, const std::string& ssp);
    
    /**
     * Parse out an IP address, port, and application tag from the
     * given ssp. Note that this potentially does a DNS lookup if the
     * string doesn't contain an explicit IP address. Also, if the url
     * did not contain a port, 0 is returned.
     *
     * @return true if the extraction was a success, false if
     * malformed
     */
    static bool parse(const std::string& ssp,
                      in_addr_t* addr, u_int16_t* port, std::string* tag);

private:
    friend class oasys::Singleton<InternetScheme>;
    InternetScheme() {}
};
    
}

#endif /* _INTERNET_SCHEME_H_ */

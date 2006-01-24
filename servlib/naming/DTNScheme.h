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
#ifndef _DTN_SCHEME_H_
#define _DTN_SCHEME_H_

#include "Scheme.h"
#include <oasys/util/Singleton.h>

namespace dtn {

/**
 * This class implements the one default scheme as specified in the
 * bundle protocol. SSPs for this scheme take the canonical forms:
 *
 * <code>
 * dtn://<router identifier>[/<application tag>]
 * dtn:none
 * </code>
 *
 * Where "router identifier" is a DNS-style "hostname" string, however
 * not necessarily a valid internet hostname, and "application tag" is
 * any string of URI-valid characters.
 *
 * The special endpoint identifier "dtn:none" is used to represent the
 * null endpoint.
 *
 * This implementation also supports limited wildcard matching for
 * endpoint patterns.
 */
class DTNScheme : public Scheme, public oasys::Singleton<DTNScheme> {
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
     * Match the pattern to the endpoint id in a scheme-specific
     * manner.
     */
    virtual bool match(const EndpointIDPattern& pattern,
                       const EndpointID& eid);
    
    /**
     * Append the given service tag to the ssp in a scheme-specific
     * manner.
     *
     * @return true if this scheme is capable of service tags and the
     * tag is a legal one, false otherwise.
     */
    virtual bool append_service_tag(std::string* ssp, const char* tag);
    
private:
    friend class oasys::Singleton<DTNScheme>;
    DTNScheme() {}
};
    
}

#endif /* _DTN_SCHEME_H_ */

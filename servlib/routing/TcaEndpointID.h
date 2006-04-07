/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * University of Waterloo Open Source License
 * 
 * Copyright (c) 2005 University of Waterloo. All rights reserved. 
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
 *   Neither the name of the University of Waterloo nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY
 * OF WATERLOO OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TCA_ENDPOINTID_H_
#define _TCA_ENDPOINTID_H_


// Warning: 
// There are 2 versions of the TcaEndpointID class, one which is a 
// subclass of EndpointID and is to be used on the servlib side.
// The other is a stand-alone class to be used on the applib side.
// The interface and functionality is otherwise the same.
// It would be nice to merge these into a single class someday.


#include <string>
#include "../../servlib/naming/EndpointID.h"

namespace dtn {


class TcaEndpointID : public EndpointID
{
public:
    TcaEndpointID() : EndpointID(), host_(""), app_("") { }
    TcaEndpointID(const EndpointID& eid);
    TcaEndpointID(const std::string& str);
    TcaEndpointID(const std::string& host, const std::string& app);
    TcaEndpointID(const TcaEndpointID& eid);

    const std::string& host() const { return host_; }
    const std::string& app() const { return app_; }
    
    void set_host(const std::string& host);
    void set_app(const std::string& app);

    const std::string get_hostid() const
        { return std::string("tca://") + host_; }

    static inline std::string
    build(const std::string& host, const std::string& app)
        { return std::string("tca://") + host + "/" + app; }
    
protected:
    // TcaEndpointID caches host part and app part for easy lookup, but
    // always maintains the base class strings as well.
    std::string host_;
    std::string app_;
    void parse();
};


} // namespace dtn

#endif /* _TCA_ENDPOINTID_H_ */

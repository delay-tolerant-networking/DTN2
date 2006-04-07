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


#include "TcaEndpointID.h"
#include "../../servlib/bundling/Bundle.h"


namespace dtn {


///////////////////////////////////////////////////////////////////////////////
// class TcaEndpointID


TcaEndpointID::TcaEndpointID(const std::string& str)
        : EndpointID(str), host_(""), app_("")
{
    parse();
}


TcaEndpointID::TcaEndpointID(const std::string& host, const std::string& app)
        : EndpointID(), host_(host), app_(app)
{
    assign(build(host, app));
}


TcaEndpointID::TcaEndpointID(const EndpointID& eid)
        : EndpointID(eid), host_(""), app_("")
{
    parse();
}


TcaEndpointID::TcaEndpointID(const TcaEndpointID& eid)
        : EndpointID(eid), host_(eid.host_), app_(eid.app_)
{
}


void
TcaEndpointID::parse()
{
    if (!valid_) return;
    if (scheme_str_ != "tca")
    {
        valid_ = false;
        return;
    }
    if (ssp_.length() <= 2)
    {
        valid_ = false;
        return;
    }
    if (ssp_.substr(0,2) != "//")
    {
        valid_ = false;
    }
    std::string nub = ssp_.substr(2, ssp_.length());

    int slash = nub.find("/");  // slash between host and app
    if (slash < 0) 
    {
        host_ = nub;    // if no slashes, assume the whole thing is host
        app_ = "";
        return;
    }

    host_ = nub.substr(0, slash);
    app_ = nub.substr(slash + 1, nub.length());
}


void
TcaEndpointID::set_host(const std::string& host)
{
    host_ = host;
    assign(build(host_, app_));
}
    

void
TcaEndpointID::set_app(const std::string& app)
{
    app_ = app;
    assign(build(host_, app_));
}


} // namespace dtn

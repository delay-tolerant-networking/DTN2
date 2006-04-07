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


///////////////////////////////////////////////////////////////////////////////
// class TcaEndpointID


TcaEndpointID::TcaEndpointID(const dtn_endpoint_id_t& eid)
        : valid_(false), host_(""), app_("")
{
    parse(eid.uri);
}


TcaEndpointID::TcaEndpointID(const std::string& str)
        : valid_(false), host_(""), app_("")
{
    parse(str);
}


TcaEndpointID::TcaEndpointID(const std::string& host, const std::string& app)
        : valid_(true), host_(host), app_(app)
{
    // TODO: we should validate host and app
}


TcaEndpointID::TcaEndpointID(const TcaEndpointID& eid)
        : valid_(true), host_(eid.host_), app_(eid.app_)
{
    // TODO: we should validate host and app
}


void
TcaEndpointID::parse(const std::string& str)
{
    // DK: We should use the dtn_parse_eid_string function here to check it
    // if (!valid_) return;
    if ((str.length() <= 6))
    {
        valid_ = false;
        return;
    }
    if (str.substr(0,6) != "tca://")
    {
        valid_ = false;
        return;
    }

    std::string nub = str.substr(6, str.length());

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
}
    

void
TcaEndpointID::set_app(const std::string& app)
{
    app_ = app;
}


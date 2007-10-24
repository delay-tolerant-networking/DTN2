/*
 *    Copyright 2005-2006 University of Waterloo
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

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


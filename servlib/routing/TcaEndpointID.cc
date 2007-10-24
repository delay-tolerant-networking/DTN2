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
    if (scheme_str() != "tca")
    {
        valid_ = false;
        return;
    }
    if (ssp().length() <= 2)
    {
        valid_ = false;
        return;
    }
    if (ssp().substr(0,2) != "//")
    {
        valid_ = false;
    }
    std::string nub = ssp().substr(2, ssp().length());

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
